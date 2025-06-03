#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Gfx.HAL.Shared.h>
#include <XEngine.Gfx.HAL.ShaderCompiler.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.SourceFileCache.h"

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	namespace BuildCacheIndexFormat
	{
		// File structure:
		//	- Header
		//	- ShaderTable (ShaderRecord x header.shaderCount)
		//	- SourceFileTable (SourceFileRecord x header.sourceFileCount)
		//	- ShaderSourceFilesIndirectionTable (uint16 x header.shaderSourceFilesIndirectionTableSize)
		//	- StringTable (uint32 x stringTableSize)
		//	- StringPool (header.stringPoolSize bytes)
		//
		// All sections are aligned to 8 bytes.
		// StringTable stores offsets pointing to locations within the StringPool.
		// String length is not stored explicitly; it is computed as the difference between the current and next string offsets stored in StringTable.
		//
		// Library file path & mod time can be invalid. This means that there was no library file produced during build.

		static constexpr uint32 SectionsAlignment = 8;

		struct Header // 48 bytes
		{
			uint32 magic;
			uint32 fileSize;
			uint16 shaderCount;
			uint16 sourceFileCount;
			uint32 shaderSourceFilesIndirectionTableSize;
			uint32 stringTableSize;
			uint32 stringPoolSize;

			uint32 manifestFilePathStringIndex;
			uint32 libraryFilePathStringIndex;
			uint64 manifestFileModTime;
			uint64 libraryFileModTime;
		};

		struct ShaderRecord // 32 bytes
		{
			uint64 nameXSH;
			uint32 pipelineLayoutNameXSH;
			uint32 pipelineLayoutHash;
			uint32 entryPointNameHash;
			uint32 definesHash;
			uint32 sourceFilesIndirectionTableOffset;
			uint16 sourceFileCount;
			HAL::ShaderType type;
		};

		struct SourceFileRecord // 12 bytes
		{
			uint32 pathStringIndex;
			uint32 modTime0;
			uint32 modTime1;
		};
	}

	class BuildCacheIndexReader : public XLib::NonCopyable
	{
	private:
		const void* fileData = nullptr;
		const BuildCacheIndexFormat::Header* header = nullptr;
		const BuildCacheIndexFormat::ShaderRecord* shaderTable = nullptr;
		const BuildCacheIndexFormat::SourceFileRecord* sourceFileTable = nullptr;
		const uint16* shaderSourceFilesIndirectionTable = nullptr;
		const uint32* stringTable = nullptr;
		const char* stringPool = nullptr;

	private:
		XLib::StringViewASCII getString(uint32 stringIndex) const;

	public:
		BuildCacheIndexReader() = default;
		~BuildCacheIndexReader();

		bool loadFromFile(const char* pathCStr, uint32 expectedMagic);

		inline uint16 getShaderCount() const { return header->shaderCount; }
		inline uint16 getSourceFileCount() const { return header->sourceFileCount; }

		inline XLib::StringViewASCII getManifestFilePath() const { return getString(header->manifestFilePathStringIndex); }
		inline uint64 getManifestFileModTime() const { return header->manifestFileModTime; }

		inline XLib::StringViewASCII getLibraryFilePath() const { return header->libraryFilePathStringIndex == uint32(-1) ? XLib::StringViewASCII() : getString(header->libraryFilePathStringIndex); }
		inline uint64 getLibraryFileModTime() const { return header->libraryFileModTime; }

		uint16 getShaderSourceFileCount(uint16 shaderIndex) const;
		uint16 getShaderSourceFileGlobalIndex(uint16 shaderIndex, uint16 shaderSourceFileIndex) const;

		XLib::StringViewASCII getSourceFilePath(uint16 globalSourceFileIndex) const;
		uint64 getSourceFileModTime(uint16 globalSourceFileIndex) const;

		uint16 findShader(uint64 shaderNameXSH) const; // returns -1 if shader not found.

		bool doShaderDetailsMatch(uint16 shaderIndex,
			uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
			const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs) const;

		inline bool isLoaded() const { return header != nullptr; }
	};

	class BuildCacheIndexWriter : public XLib::NonCopyable
	{
	private:
		struct Shader
		{
			uint64 nameXSH;
			uint32 pipelineLayoutNameXSH;
			uint32 pipelineLayoutHash;
			uint32 entryPointNameHash;
			uint32 definesHash;
			uint32 globalSourceFilesStoreOffset;
			uint16 sourceFileCount;
			HAL::ShaderType type;
		};

	private:
		XLib::ArrayList<Shader> shaders;
		XLib::ArrayList<SourceFileHandle> globalShaderSourceFilesStore;

	public:
		BuildCacheIndexWriter() = default;
		~BuildCacheIndexWriter() = default;

		void addShader(uint64 nameXSH,
			uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
			const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs,
			const SourceFileHandle* sourceFiles, uint16 sourceFileCount);

		void buildAndStoreToFile(const char* pathCStr, uint32 magic, const SourceFileCache& sourceFileCache,
			const char* manifestFilePathCStr, const char* libraryFilePathCStr);

		inline void reserveShaderCount(uint32 shaderCount) { shaders.reserve(shaderCount); }
	};
}
