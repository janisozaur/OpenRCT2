package io.openrct2;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import org.apache.commons.io.IOUtils;

public class ZipArchive
{
    private ZipFile _zipArchive;
    private List<String> _entryNames;
    private Map<String, byte[]> _entries;

    public ZipArchive(Context context, String path) throws IOException
    {
        if (path.startsWith("/android_asset/"))
        {
            String assetPath = path.substring(15);
            AssetManager assetManager = context.getAssets();
            try (InputStream is = assetManager.open(assetPath); ZipInputStream zis = new ZipInputStream(is))
            {
                _entryNames = new ArrayList<>();
                _entries = new HashMap<>();
                ZipEntry entry;
                while ((entry = zis.getNextEntry()) != null)
                {
                    byte[] data = IOUtils.toByteArray(zis);
                    _entryNames.add(entry.getName());
                    _entries.put(entry.getName().toLowerCase(), data);
                    zis.closeEntry();
                }
            }
        }
        else
        {
            _zipArchive = new ZipFile(path);
        }
    }

    public void close()
    {
        try
        {
            _zipArchive.close();
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    public int getNumFiles()
    {
        if (_zipArchive != null)
        {
            return _zipArchive.size();
        }
        return _entryNames.size();
    }

    private ZipEntry getZipEntry(int index)
    {
        if (_zipArchive == null)
        {
            return null;
        }
        Enumeration<? extends ZipEntry> entries = _zipArchive.entries();

        int i = 0;
        while (entries.hasMoreElements())
        {
            ZipEntry entry = entries.nextElement();
            if (index == i)
            {
                return entry;
            }

            i++;
        }

        return null;
    }

    public String getFileName(int index)
    {
        if (_zipArchive != null)
        {
            ZipEntry entry = getZipEntry(index);
            if (entry != null)
            {
                return entry.getName();
            }
        }
        else
        {
            if (index >= 0 && index < _entryNames.size())
            {
                return _entryNames.get(index);
            }
        }

        return null;
    }

    public long getFileSize(int index)
    {
        if (_zipArchive != null)
        {
            ZipEntry entry = getZipEntry(index);
            if (entry != null)
            {
                return entry.getSize();
            }
        }
        else
        {
            if (index >= 0 && index < _entryNames.size())
            {
                byte[] data = _entries.get(_entryNames.get(index).toLowerCase());
                return data != null ? data.length : 0;
            }
        }

        return -1;
    }

    public int getFileIndex(String path)
    {
        if (_zipArchive != null)
        {
            Enumeration<? extends ZipEntry> entries = _zipArchive.entries();

            int i = 0;
            while (entries.hasMoreElements())
            {
                ZipEntry entry = entries.nextElement();
                if (entry.getName().equalsIgnoreCase(path))
                {
                    return i;
                }

                i++;
            }
        }
        else
        {
            for (int i = 0; i < _entryNames.size(); i++)
            {
                if (_entryNames.get(i).equalsIgnoreCase(path))
                {
                    return i;
                }
            }
        }

        return -1;
    }

    public byte[] getFile(int index) throws IOException
    {
        if (_zipArchive != null)
        {
            ZipEntry entry = getZipEntry(index);

            if (entry == null)
            {
                return null;
            }

            try (InputStream inputStream = _zipArchive.getInputStream(entry))
            {
                int numBytesToRead = (int)entry.getSize();
                if (numBytesToRead == -1)
                {
                    Log.e("ZipArchive", "Unknown length for zip entry");
                    return null;
                }

                byte[] inBuffer = new byte[numBytesToRead];
                IOUtils.read(inputStream, inBuffer);
                return inBuffer;
            }
        }
        else
        {
            if (index >= 0 && index < _entryNames.size())
            {
                return _entries.get(_entryNames.get(index).toLowerCase());
            }
        }
        return null;
    }
}
