/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../platform/Platform.h"
#include "FileStream.h"
#include "Path.hpp"
#include "String.hpp"

#include <string_view>

#ifndef _WIN32
    #include <sys/stat.h>
#else
    #include <io.h>
#endif

#ifdef _MSC_VER
    #define ftello _ftelli64
    #define fseeko _fseeki64
#endif

#ifdef __ANDROID__
    #include <android/asset_manager.h>
#endif

namespace OpenRCT2
{
    FileStream::FileStream(const fs::path& path, FileMode fileMode)
        : FileStream(path.u8string(), fileMode)
    {
    }

    FileStream::FileStream(const std::string& path, FileMode fileMode)
        : FileStream(path.c_str(), fileMode)
    {
    }

    FileStream::FileStream(std::string_view path, FileMode fileMode)
        : FileStream(std::string(path), fileMode)
    {
    }

    FileStream::FileStream(const utf8* path, FileMode fileMode)
    {
#ifdef __ANDROID__
        if (fileMode == FileMode::open && String::startsWith(path, "/android_asset/"))
        {
            auto assetManager = static_cast<AAssetManager*>(Platform::GetAssetManager());
            if (assetManager != nullptr)
            {
                std::string assetPath = path + 15;
                _asset = AAssetManager_open(assetManager, assetPath.c_str(), AASSET_MODE_RANDOM);
                if (_asset != nullptr)
                {
                    _fileSize = AAsset_getLength64(static_cast<AAsset*>(_asset));
                    _canRead = true;
                    _canWrite = false;
                    _ownsFilePtr = true;
                    return;
                }
            }
        }
#endif

        const char* mode;
        switch (fileMode)
        {
            case FileMode::open:
                mode = "rb";
                _canRead = true;
                _canWrite = false;
                break;
            case FileMode::write:
                mode = "w+b";
                _canRead = true;
                _canWrite = true;
                break;
            case FileMode::append:
                mode = "a";
                _canRead = false;
                _canWrite = true;
                break;
            default:
                throw;
        }

        // Make sure the directory exists before writing to a file inside it
        if (_canWrite)
        {
            std::string directory = Path::GetDirectory(path);
            if (!Path::DirectoryExists(directory))
            {
                Path::CreateDirectory(directory);
            }
        }

#ifdef _WIN32
        auto pathW = String::toWideChar(path);
        auto modeW = String::toWideChar(mode);
        _file = _wfopen(pathW.c_str(), modeW.c_str());
#else
        if (fileMode == FileMode::open)
        {
            struct stat fileStat;
            // Only allow regular files to be opened as its possible to open directories.
            if (stat(path, &fileStat) == 0 && S_ISREG(fileStat.st_mode))
            {
                _file = fopen(path, mode);
            }
        }
        else
        {
            _file = fopen(path, mode);
        }
#endif
        if (_file == nullptr)
        {
            throw IOException(String::stdFormat("Unable to open '%s'", path));
        }

#ifdef _WIN32
        _fileSize = _filelengthi64(_fileno(_file));
#else
        std::error_code ec;
        _fileSize = fs::file_size(fs::u8path(path), ec);
#endif

        _ownsFilePtr = true;
    }

    FileStream::~FileStream()
    {
        if (!_disposed)
        {
            _disposed = true;
            if (_ownsFilePtr)
            {
#ifdef __ANDROID__
                if (_asset != nullptr)
                {
                    AAsset_close(static_cast<AAsset*>(_asset));
                }
                else
#endif
                {
                    if (_file != nullptr)
                    {
                        fclose(_file);
                    }
                }
            }
        }
    }

    bool FileStream::CanRead() const
    {
        return _canRead;
    }

    bool FileStream::CanWrite() const
    {
        return _canWrite;
    }

    uint64_t FileStream::GetLength() const
    {
        return _fileSize;
    }

    uint64_t FileStream::GetPosition() const
    {
#ifdef __ANDROID__
        if (_asset != nullptr)
        {
            return AAsset_seek64(static_cast<AAsset*>(_asset), 0, SEEK_CUR);
        }
#endif
        return ftello(_file);
    }

    void FileStream::SetPosition(uint64_t position)
    {
        Seek(position, STREAM_SEEK_BEGIN);
    }

    void FileStream::Seek(int64_t offset, int32_t origin)
    {
#ifdef __ANDROID__
        if (_asset != nullptr)
        {
            int whence;
            switch (origin)
            {
                case STREAM_SEEK_BEGIN:
                    whence = SEEK_SET;
                    break;
                case STREAM_SEEK_CURRENT:
                    whence = SEEK_CUR;
                    break;
                case STREAM_SEEK_END:
                    whence = SEEK_END;
                    break;
                default:
                    return;
            }
            AAsset_seek64(static_cast<AAsset*>(_asset), offset, whence);
            return;
        }
#endif
        switch (origin)
        {
            case STREAM_SEEK_BEGIN:
                fseeko(_file, offset, SEEK_SET);
                break;
            case STREAM_SEEK_CURRENT:
                fseeko(_file, offset, SEEK_CUR);
                break;
            case STREAM_SEEK_END:
                fseeko(_file, offset, SEEK_END);
                break;
        }
    }

    void FileStream::Read(void* buffer, uint64_t length)
    {
#ifdef __ANDROID__
        if (_asset != nullptr)
        {
            if (static_cast<uint64_t>(AAsset_read(static_cast<AAsset*>(_asset), buffer, static_cast<size_t>(length))) == length)
            {
                return;
            }
            throw IOException("Attempted to read past end of file.");
        }
#endif
        if (fread(buffer, 1, static_cast<size_t>(length), _file) == length)
        {
            return;
        }
        throw IOException("Attempted to read past end of file.");
    }

    void FileStream::Write(const void* buffer, uint64_t length)
    {
        if (length == 0)
        {
            return;
        }
        if (auto count = fwrite(buffer, static_cast<size_t>(length), 1, _file); count != 1)
        {
            std::string error = "Unable to write " + std::to_string(length) + " bytes to file. Count = " + std::to_string(count)
                + ", errno = " + std::to_string(errno);
            throw IOException(error);
        }

        uint64_t position = GetPosition();
        _fileSize = std::max(_fileSize, position);
    }

    uint64_t FileStream::TryRead(void* buffer, uint64_t length)
    {
#ifdef __ANDROID__
        if (_asset != nullptr)
        {
            return AAsset_read(static_cast<AAsset*>(_asset), buffer, static_cast<size_t>(length));
        }
#endif
        size_t readBytes = fread(buffer, 1, static_cast<size_t>(length), _file);
        return readBytes;
    }

} // namespace OpenRCT2
