#include <unordered_map>

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.System.File.h>

#include <XEngine.Gfx.HAL.Shared.h>
#include <XEngine.Gfx.HAL.ShaderCompiler.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.SourceFileCache.h"

// BuildDepsTrace file stores the list of build dependencies.
// It is used to track the validity of compiled shaders and the library file as a whole.

namespace XEngine::Gfx::ShaderLibraryBuilder::BuildDepsTrace
{
	namespace Format
	{
		// The file is composed of a sequence of records and supports truncation at any point.
		// Records integrity is verified using CRC hashes.
		//
		// The first record is always a ManifestFileRecord.
		// A SourceFileRecord declares a source file.
		// A ShaderRecord declares a shader and may reference previously declared source files
		// via their indices (used as uint16 identifiers) in its list of dependencies.
		//
		// On a successful build, the file always ends with a BuiltLibraryFileRecord.
		// On a failed build, the file may be truncated before this final record.
		//
		// Shader can exceed source file count limit. In this case, no source files will be saved,
		// and shader will be considred invalidated.

		static constexpr uint16 RecordAlignment = 4;
		static constexpr uint16 MaxRecordSize = 2048;
		static constexpr uint16 RecordTypeHeaderFieldBitCount = 3;
		static constexpr uint16 RecordSizeHeaderFieldBitCount = 13;

		enum class RecordType : uint16
		{
			Undefined = 0,
			ManifestFile,
			SourceFile,
			Shader,
			BuiltLibraryFile,

			ValueCount,
		};

		struct Header // 4 bytes
		{
			uint32 magic;
		};

		struct RecordHeader // 4 bytes
		{
			RecordType type : RecordTypeHeaderFieldBitCount;
			uint16 size : RecordSizeHeaderFieldBitCount;
			uint16 hash;
		};

		struct ManifestFileRecordBody // 12 bytes
		{
			uint32 modTime0;
			uint32 modTime1;
			uint16 pathLength;
			uint16 _padding;
		};

		struct SourceFileRecordBody // 12 bytes
		{
			uint32 modTime0;
			uint32 modTime1;
			uint16 index;
			uint16 pathLength;
		};

		struct ShaderRecordBody // 36 bytes
		{
			uint32 nameXSH0;
			uint32 nameXSH1;
			uint32 pipelineLayoutNameXSH;
			uint32 pipelineLayoutHash;
			uint32 entryPointNameHash;
			uint32 definesHash;
			uint32 compiledBlobFileModTime0;
			uint32 compiledBlobFileModTime1;
			uint16 sourceFileCount;
			HAL::ShaderType type;
			uint8 _padding;
		};

		struct BuiltLibraryFileRecordBody // 12 bytes
		{
			uint32 modTime0;
			uint32 modTime1;
			uint16 pathLength;
			uint16 _padding;
		};

		static_assert(MaxRecordSize < (1 << RecordSizeHeaderFieldBitCount));
		static_assert(uint16(RecordType::ValueCount) <= (1 << RecordTypeHeaderFieldBitCount));
		static_assert(sizeof(Format::Header) % Format::RecordAlignment == 0);
		static_assert(sizeof(Format::RecordHeader) == 4);
	}

	class Reader : public XLib::NonCopyable
	{
	private:
		struct Shader
		{
			uint64 nameXSH;
			const Format::ShaderRecordBody* record;
		};

	private:
		const void* fileData = nullptr;
		const Format::ManifestFileRecordBody* manifestFileRecord = nullptr;
		const Format::BuiltLibraryFileRecordBody* builtLibraryFileRecord = nullptr;
		XLib::ArrayList<const Format::SourceFileRecordBody*> sourceFiles;
		XLib::ArrayList<Shader> shaders;

	public:
		Reader() = default;
		~Reader();

		bool load(const char* pathCStr, uint32 expectedMagic);

		inline bool isLoaded() const { return fileData != nullptr; }
		inline bool wasBuildSuccessful() const { return builtLibraryFileRecord != nullptr; }
		inline uint16 getSourceFileCount() const { return uint16(sourceFiles.getSize()); }
		inline uint16 getShaderCount() const { return uint16(shaders.getSize()); }

		XLib::StringViewASCII getManifestFilePath() const;
		uint64 getManifestFileModTime() const;

		XLib::StringViewASCII getBuiltLibraryFilePath() const;
		uint64 getBuiltLibraryFileModTime() const;

		XLib::StringViewASCII getSourceFilePath(uint16 sourceFileIndex) const;
		uint64 getSourceFileModTime(uint16 sourceFileIndex) const;

		uint16 getShaderSourceFileCount(uint16 shaderIndex) const;
		uint16 getShaderSourceFileIndex(uint16 shaderIndex, uint16 localSourceFileIndex) const;
		uint64 getShaderCompiledBlobModTime(uint16 shaderIndex) const;

		uint16 findShader(uint64 shaderNameXSH) const; // returns -1 if shader not found.

		bool doShaderDetailsMatch(uint16 shaderIndex,
			uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
			const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs) const;
	};

	class Writer : public XLib::NonCopyable
	{
	private:
		static constexpr uint16 BufferSize = Format::MaxRecordSize * 2;
		static constexpr uint16 MaxShaderSourceFileCount = 256;
		static constexpr float32 MaxFlushIntervalSeconds = 2.0f;

		struct SourceFileData
		{
			uint16 index;
			uint16 lastDependentShaderIndex;
		};

	private:
		XLib::File file;

		std::unordered_map<SourceFileHandle, SourceFileData> allTrackedSourceFiles;
		SourceFileCache* sourceFileCache = nullptr;

		byte* buffer = nullptr;
		uint16 bufferUsedSize = 0;
		uint16 shaderCount = 0;

		uint64 lastFlushTimestamp = 0;

	private:
		void putRecord(Format::RecordType type,
			const void* bodyData, uintptr bodySize,
			const void* payloadData, uintptr payloadSize,
			bool nullTerminatePayload = false);

		void flush();
		void cleanup();

	public:
		Writer() = default;
		inline ~Writer() { closeAfterFailedBuild(); }

		void openForWriting(const char* pathCStr, uint32 magic,
			XLib::StringViewASCII manifestFilePath, uint64 manifestFileModTime, SourceFileCache& sourceFileCache);

		void addShader(uint64 nameXSH,
			uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
			const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs,
			const SourceFileHandle* sourceFiles, uint16 sourceFileCount, uint64 compiledBlobModTime);

		void closeAfterSuccessfulBuild(XLib::StringViewASCII builtLibraryFilePath, uint64 builtLibraryFileModTime);
		void closeAfterFailedBuild();
	};
}
