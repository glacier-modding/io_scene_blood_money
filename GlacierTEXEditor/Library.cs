using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.IO.Compression;
using System.Drawing.Imaging;
using StbImageWriteSharp;
using GlacierTEXEditor.FileFormats;
using StbImageSharp;
using DirectXTexNet;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Reflection;

namespace GlacierTEXEditor
{
    public class Interface
    {
        public static Library lib = new Library();

        [DllExport]
        public static IntPtr LoadTex(IntPtr buffer, UInt32 size)
        {
            byte[] data = new byte[size];
            Marshal.Copy(buffer, data, 0, (int)size);
            lib = new Library();
            lib.memoryStream = new MemoryStream(data, 0, (int)size, true, true);
            lib.ReadTexFile();
            lib.json_bytes = Encoding.ASCII.GetBytes(lib.json);
            lib.unmanagedPointer = Marshal.AllocHGlobal(lib.json_bytes.Length);
            Marshal.Copy(lib.json_bytes, 0, lib.unmanagedPointer, lib.json_bytes.Length);
            return lib.unmanagedPointer;
        }

        [DllExport]
        public static int UnLoadTex()
        {
            lib = new Library();
            return 0;
        }

        [DllExport]
        public static int FreeJson()
        {
            Marshal.FreeHGlobal(lib.unmanagedPointer);
            return 0;
        }

        [DllExport]
        public static int WriteTga(string dir, string file, Int32 texture_id)
        {
            return lib.ExportFile(dir, file, texture_id) == true ? 1 : 0;
        }
    }

    public class Library
    {
        public IntPtr unmanagedPointer;
        public byte[] json_bytes;
        public string json = "";
        public MemoryStream memoryStream = new MemoryStream();
        public TEX tex = new TEX();
        public List<Texture> textures = new List<Texture>();
        public Dictionary<int, List<Texture>> texturesBackup = new Dictionary<int, List<Texture>>();
        public int countOfEmptyOffsets = 0;

        public int entriesCount = 0;
        public Dictionary<string, int> entryTypesCount = new Dictionary<string, int>();
        public int indexOfLastModifiedTexture = 0;
        public int indexOfCurrentBackup = 0;
        public CompressionLevel compressionLevel;
        public GameVersion gameVersion;

        public void ReadTexFile()
        {
            json = "{";

            if (textures.Count > 0)
            {
                textures.Clear();
                countOfEmptyOffsets = 0;
                entriesCount = 0;
                entryTypesCount.Clear();
            }

            using (BinaryReader binaryReader = new BinaryReader(memoryStream))
            {
                if (gameVersion != GameVersion.PS4)
                {
                    tex.texHeader.table1Offset = binaryReader.ReadUInt32();
                    tex.texHeader.table2Offset = binaryReader.ReadUInt32();
                    tex.texHeader.unk1 = binaryReader.ReadUInt32();
                    tex.texHeader.unk2 = binaryReader.ReadUInt32();

                    //Console.WriteLine("Table 1 Offset: " + tex.texHeader.table1Offset.ToString("X2"));
                    //Console.WriteLine("Table 2 Offset: " + tex.texHeader.table2Offset.ToString("X2"));

                    memoryStream.Seek(tex.texHeader.table1Offset, SeekOrigin.Begin);

                    tex.table1Offsets = new uint[2048];

                    for (int i = 0; i < tex.table1Offsets.Length; i++)
                    {
                        tex.table1Offsets[i] = binaryReader.ReadUInt32();
                        //Console.WriteLine("Table Offset: " + tex.table1Offsets[i].ToString("X2"));
                    }
                }
                else
                {
                    memoryStream.Seek(0x210, SeekOrigin.Begin);

                    int offsetsCount = (0x2010 - 0x210) / 4;
                    tex.table1Offsets = new uint[offsetsCount];

                    for (int i = 0; i < tex.table1Offsets.Length; i++)
                    {
                        tex.table1Offsets[i] = binaryReader.ReadUInt32();
                    }
                }

                tex.entries = new List<TEXEntry>();

                for (int i = 0; i < tex.table1Offsets.Length; i++)
                {
                    if (tex.table1Offsets[i] != 0)
                    {
                        memoryStream.Seek(tex.table1Offsets[i], SeekOrigin.Begin);

                        Texture texture = new Texture();

                        texture.Offset = (int)tex.table1Offsets[i];
                        texture.FileSize = binaryReader.ReadInt32();

                        string type1 = Encoding.Default.GetString(binaryReader.ReadBytes(4));
                        string type2 = Encoding.Default.GetString(binaryReader.ReadBytes(4));

                        texture.Type1 = ReverseTypeText(type1);
                        texture.Type2 = ReverseTypeText(type2);

                        texture.Index = binaryReader.ReadInt32();

                        texture.Height = binaryReader.ReadInt16();
                        texture.Width = binaryReader.ReadInt16();
                        texture.NumOfMipMips = binaryReader.ReadInt32();
                        texture.Data = new byte[texture.NumOfMipMips][];
                        texture.DataOffsets = new long[texture.NumOfMipMips];
                        texture.MipMapLevelSizes = new int[texture.NumOfMipMips];

                        texture.Unk1 = binaryReader.ReadInt32();
                        texture.Unk2 = binaryReader.ReadInt32();
                        texture.Unk3 = binaryReader.ReadInt32();

                        byte letter;

                        texture.FileName = "";

                        while ((letter = binaryReader.ReadByte()) != 0)
                        {
                            texture.FileName += Encoding.Default.GetString(new byte[] { letter });
                        }

                        texture.FileName = texture.FileName.Replace("'", "").Replace("\\", "");

                        json += "\"";
                        json += texture.Index.ToString();
                        json += "\":\"";
                        json += texture.FileName;
                        json += "\",";

                        for (int j = 0; j < texture.Data.Length; j++)
                        {
                            int size = binaryReader.ReadInt32();

                            texture.MipMapLevelSizes[j] = size;
                            texture.DataOffsets[j] = binaryReader.BaseStream.Position;
                            texture.Data[j] = binaryReader.ReadBytes(size);
                        }

                        if (texture.Type1.Equals("PALN"))
                        {
                            texture.PaletteSize = binaryReader.ReadInt32();
                            int dataSize = texture.PaletteSize * 4;
                            texture.PalData = binaryReader.ReadBytes(dataSize);
                        }

                        entriesCount += 1;

                        if (entryTypesCount.ContainsKey(texture.Type1))
                        {
                            entryTypesCount[texture.Type1] += 1;
                        }
                        else
                        {
                            entryTypesCount.Add(texture.Type1, 1);
                        }

                        textures.Add(texture);
                    }
                    else
                    {
                        if (textures.Count == 0)
                        {
                            countOfEmptyOffsets++;
                        }
                    }
                }

                if (gameVersion != GameVersion.PS4)
                {
                    memoryStream.Seek(tex.texHeader.table2Offset, SeekOrigin.Begin);

                    tex.table2Offsets = new uint[2048];

                    for (int i = 0; i < tex.table2Offsets.Length; i++)
                    {
                        tex.table2Offsets[i] = binaryReader.ReadUInt32();
                        long position = binaryReader.BaseStream.Position;

                        if (tex.table2Offsets[i] != 0)
                        {
                            memoryStream.Seek(tex.table2Offsets[i], SeekOrigin.Begin);

                            int indicesCount = binaryReader.ReadInt32();
                            int[] indices = new int[indicesCount];

                            for (int j = 0; j < indices.Length; j++)
                            {
                                indices[j] = binaryReader.ReadInt32();
                            }

                            int index = 0;

                            if (indices[indices.Length - 1] == 0)
                            {
                                for (int j = indices.Length - 1; j >= 0; j--)
                                {
                                    if (indices[j] > 0)
                                    {
                                        index = indices[j] - countOfEmptyOffsets;
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                index = indices[indices.Length - 1] - countOfEmptyOffsets;
                            }

                            textures[index].IndicesCount = indicesCount;
                            textures[index].Indices = new int[indicesCount];

                            Array.Copy(indices, 0, textures[index].Indices, 0, indicesCount);

                            memoryStream.Seek(position, SeekOrigin.Begin);
                        }
                    }
                }
            }

            json = json.Trim(',');

            json += "}";
        }

        public void CreateBackupOfTexture(Texture texture)
        {
            Texture textureBackup = ObjectExtensions.Clone(texture);

            if (!texturesBackup.ContainsKey(texture.Index))
            {
                texturesBackup.Add(texture.Index, new List<Texture>());
            }

            texturesBackup[texture.Index].Add(textureBackup);
            indexOfCurrentBackup++;
        }

        public bool CheckDDSFileHeader(string path, string type)
        {
            try
            {
                using (FileStream fileStream = new FileStream(path, FileMode.Open, FileAccess.Read))
                {
                    using (BinaryReader binaryReader = new BinaryReader(fileStream))
                    {
                        string magic = Encoding.Default.GetString(binaryReader.ReadBytes(4));
                        int size = binaryReader.ReadInt32();
                        int flags = binaryReader.ReadInt32();

                        if (!magic.Equals("DDS ") || size != 124)
                        {
                            MessageBox.Show("Source file is not a DDS file.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                            return false;
                        }

                        uint expectedFlags = (uint)DDSSurfaceDescFlags.DDSD_CAPS | (uint)DDSSurfaceDescFlags.DDSD_HEIGHT
                            | (uint)DDSSurfaceDescFlags.DDSD_WIDTH | (uint)DDSSurfaceDescFlags.DDSD_PIXELFORMAT;

                        if (!type.Equals("A8B8G8R8") && !type.Equals("L8"))
                        {
                            expectedFlags |= (uint)DDSSurfaceDescFlags.DDSD_LINEARSIZE;
                        }
                        else
                        {
                            expectedFlags |= (uint)DDSSurfaceDescFlags.DDSD_PITCH;
                        }

                        if ((flags & expectedFlags) != expectedFlags)
                        {
                            MessageBox.Show("Bad flags in DDS file.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                            return false;
                        }

                        int height = binaryReader.ReadInt32();
                        int width = binaryReader.ReadInt32();
                        int pitchOrLinearSize = binaryReader.ReadInt32();
                        int depth = binaryReader.ReadInt32();
                        int mipMapCount = binaryReader.ReadInt32();

                        if (mipMapCount > 1 && ((flags & (uint)DDSSurfaceDescFlags.DDSD_MIPMAPCOUNT) != (uint)DDSSurfaceDescFlags.DDSD_MIPMAPCOUNT))
                        {
                            MessageBox.Show("Missing MIPMAPCOUNT flag.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                            return false;
                        }

                        for (int i = 0; i < 11; i++)
                        {
                            binaryReader.ReadInt32();
                        }

                        int size2 = binaryReader.ReadInt32();

                        if (size2 != 32)
                        {
                            MessageBox.Show("Bad flags in DDS Pixel Format.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                            return false;
                        }

                        int flags2 = binaryReader.ReadInt32();
                        string fourCC = Encoding.Default.GetString(binaryReader.ReadBytes(4));
                        uint expectedFlags2;

                        if (type.Equals("A8B8G8R8"))
                        {
                            expectedFlags2 = (uint)DDSPixelFormatFlags.DDPF_RGB | (uint)DDSPixelFormatFlags.DDPF_ALPHAPIXELS;

                            if ((flags2 & expectedFlags2) != expectedFlags2)
                            {
                                MessageBox.Show("Bad flags in DDS Pixel Format.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                return false;
                            }

                            uint rgbBitCount = binaryReader.ReadUInt32();
                            uint dwRBitMask = binaryReader.ReadUInt32();
                            uint dwGBitMask = binaryReader.ReadUInt32();
                            uint dwBBitMask = binaryReader.ReadUInt32();
                            uint dwABitMask = binaryReader.ReadUInt32();

                            if (rgbBitCount != 32 || dwRBitMask != 0xFF0000 || dwGBitMask != 0xFF00 || dwBBitMask != 0xFF
                                || dwABitMask != 0xFF000000)
                            {
                                MessageBox.Show("Bad flags in DDS Pixel Format.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                return false;
                            }
                        }
                        else if (type.Equals("L8"))
                        {
                            expectedFlags2 = (uint)DDSPixelFormatFlags.DDPF_LUMINANCE;

                            if ((flags2 & expectedFlags2) != expectedFlags2)
                            {
                                MessageBox.Show("Bad flags in DDS Pixel Format.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                return false;
                            }

                            uint rgbBitCount = binaryReader.ReadUInt32();
                            uint dwRBitMask = binaryReader.ReadUInt32();
                            uint dwGBitMask = binaryReader.ReadUInt32();
                            uint dwBBitMask = binaryReader.ReadUInt32();
                            uint dwABitMask = binaryReader.ReadUInt32();

                            if (rgbBitCount != 8 || dwRBitMask != 0xFF || dwGBitMask != 0 || dwBBitMask != 0
                                || dwABitMask != 0)
                            {
                                MessageBox.Show("Bad flags in DDS Pixel Format.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                return false;
                            }
                        }
                        else
                        {
                            if (flags2 != (uint)DDSPixelFormatFlags.DDPF_FOURCC || !type.Equals(fourCC))
                            {
                                char dxtNum = char.Parse(type.Substring(type.Length - 1));
                                MessageBox.Show("DXT" + dxtNum + " compression expected in DDS file (incompatible format found).",
                                    "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                return false;
                            }

                            for (int i = 0; i < 5; i++)
                            {
                                binaryReader.ReadInt32();
                            }
                        }

                        uint caps1 = binaryReader.ReadUInt32();
                        uint expectedCaps = (uint)DDSHeaderCaps.DDSCAPS_TEXTURE;

                        if (mipMapCount > 1)
                        {
                            expectedCaps |= (uint)DDSHeaderCaps.DDSCAPS_COMPLEX | (uint)DDSHeaderCaps.DDSCAPS_MIPMAP;
                        }

                        if (caps1 != expectedCaps)
                        {
                            MessageBox.Show("Bad flags in Surface Format Header.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                            return false;
                        }
                    }
                }
            }
            catch (IOException ex)
            {
                MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }

            return true;
        }

        public bool ImportDDSFile(Texture texture, string path, string type)
        {
            if (!CheckDDSFileHeader(path, type))
            {
                return false;
            }

            try
            {
                using (FileStream fileStream = new FileStream(path, FileMode.Open, FileAccess.Read))
                {
                    using (BinaryReader binaryReader = new BinaryReader(fileStream))
                    {
                        string magic = Encoding.Default.GetString(binaryReader.ReadBytes(4));
                        int size = binaryReader.ReadInt32();
                        int flags = binaryReader.ReadInt32();
                        int height = binaryReader.ReadInt32();
                        int width = binaryReader.ReadInt32();
                        int pitchOrLinearSize = binaryReader.ReadInt32();
                        int depth = binaryReader.ReadInt32();
                        int mipMapCount = binaryReader.ReadInt32();

                        for (int i = 0; i < 11; i++)
                        {
                            binaryReader.ReadInt32();
                        }

                        int size2 = binaryReader.ReadInt32();
                        int flags2 = binaryReader.ReadInt32();
                        string fourCC = Encoding.Default.GetString(binaryReader.ReadBytes(4));

                        for (int i = 0; i < 10; i++)
                        {
                            binaryReader.ReadInt32();
                        }

                        texture.Width = width;
                        texture.Height = height;

                        texture.NumOfMipMips = mipMapCount;
                        texture.MipMapLevelSizes = new int[mipMapCount];

                        int mainImageSize = CalculateMainImageSize(texture.Type1, width, height);

                        texture.MipMapLevelSizes[0] = mainImageSize;
                        int n = 0;

                        for (int i = 1; i < mipMapCount; i++)
                        {
                            int mipMapSize = texture.MipMapLevelSizes[i - 1] / 4;

                            if (texture.Type1.Equals("DXT1") && mipMapSize <= 8)
                            {
                                n = i;
                                break;
                            }
                            else if (texture.Type1.Equals("DXT3") && mipMapSize <= 16)
                            {
                                n = i;
                                break;
                            }

                            texture.MipMapLevelSizes[i] = mipMapSize;
                        }

                        if (texture.Type1.Equals("DXT1"))
                        {
                            while (n < mipMapCount)
                            {
                                texture.MipMapLevelSizes[n++] = 8;
                            }
                        }
                        else if (texture.Type1.Equals("DXT3"))
                        {
                            while (n < mipMapCount)
                            {
                                texture.MipMapLevelSizes[n++] = 16;
                            }
                        }

                        texture.Data = new byte[mipMapCount][];

                        for (int i = 0; i < mipMapCount; i++)
                        {
                            byte[] data = binaryReader.ReadBytes(texture.MipMapLevelSizes[i]);

                            if (type.Equals("A8B8G8R8"))
                            {
                                int currentWidth = width / (int)Math.Pow(2, i);
                                int currentHeight = height / (int)Math.Pow(2, i);

                                if (texture.Type1.Equals("PALN"))
                                {
                                    data = ColorsToPALNRefs(data);
                                }
                                else if (texture.Type1.Equals("RGBA"))
                                {
                                    ConvertBGRAToRGBA(data, currentWidth, currentHeight);
                                }
                            }

                            texture.Data[i] = data;
                        }

                        texture.FileSize = CalculateFileSize(texture);
                    }
                }
            }
            catch (IOException ex)
            {
                MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                return false;
            }

            return true;
        }

        public int CalculateMainImageSize(string type, int width, int height)
        {
            int mainImageSize;
            int bytesPerPixel;

            if (type.Equals("DXT1"))
            {
                mainImageSize = Math.Max(1, (width + 3) / 4) * Math.Max(1, (height + 3) / 4) * 8;
            }
            else if (type.Equals("DXT3"))
            {
                mainImageSize = Math.Max(1, (width + 3) / 4) * Math.Max(1, (height + 3) / 4) * 16;
            }
            else if (type.Equals("PALN") || type.Equals("I8  "))
            {
                bytesPerPixel = 1;
                mainImageSize = width * height * bytesPerPixel;
            }
            else if (type.Equals("U8V8"))
            {
                bytesPerPixel = 2;
                mainImageSize = width * height * bytesPerPixel;
            }
            else
            {
                bytesPerPixel = 4;
                mainImageSize = width * height * bytesPerPixel;
            }

            return mainImageSize;
        }

        public int CalculateNumOfMipMaps(int width, int height)
        {
            int numOfMipMaps = 0;

            while (width > 1 && height > 1)
            {
                width /= 2;
                height /= 2;

                numOfMipMaps++;
            }

            return numOfMipMaps;
        }


        public int CalculateFileSize(Texture texture)
        {
            int fileSize = texture.Type1.Length + texture.Type2.Length + sizeof(uint) + sizeof(ushort) + sizeof(ushort)
                + sizeof(uint) + sizeof(uint) + sizeof(uint) + sizeof(uint) + texture.FileName.Length + 1;

            for (int i = 0; i < texture.MipMapLevelSizes.Length; i++)
            {
                fileSize += texture.MipMapLevelSizes[i] + texture.Data[i].Length;
            }

            if (texture.Type1.Equals("PALN"))
            {
                fileSize += texture.PalData.Length;
            }

            if (texture.IndicesCount > 0)
            {
                fileSize += sizeof(uint) + texture.Indices.Length;
            }

            return fileSize;
        }

        public byte[][] GenerateMipMaps(Bitmap bitmap, int numOfMipMaps)
        {
            byte[][] mipMaps = new byte[numOfMipMaps - 1][];
            int n = 0;

            int width = bitmap.Width;
            int height = bitmap.Height;

            while (n < numOfMipMaps - 1)
            {
                width /= 2;
                height /= 2;

                Bitmap resized = new Bitmap(bitmap, new Size(width, height));
                byte[] data = BMPImage.BitmapToData(resized);

                mipMaps[n] = new byte[data.Length];
                Array.Copy(data, 0, mipMaps[n++], 0, data.Length);
            }

            return mipMaps;
        }

        public bool ImportDXTFile(Texture texture, string path)
        {
            CreateBackupOfTexture(texture);

            string extension = Path.GetExtension(path);
            bool imported;

            if (extension.Equals(".tga"))
            {
                using (ScratchImage image = TexHelper.Instance.LoadFromTGAFile(path))
                {
                    using (ScratchImage mipChain = image.GenerateMipMaps(TEX_FILTER_FLAGS.DEFAULT, 0))
                    {
                        DXGI_FORMAT dxgiFormat;

                        if (texture.Type1.Equals("DXT1"))
                        {
                            dxgiFormat = DXGI_FORMAT.BC1_UNORM;
                        }
                        else
                        {
                            dxgiFormat = DXGI_FORMAT.BC2_UNORM;
                        }

                        using (ScratchImage comp = mipChain.Compress(dxgiFormat, TEX_COMPRESS_FLAGS.PARALLEL, 0.5f))
                        {
                            path = Path.Combine(GetOutputPath(), "compressed.dds");
                            comp.SaveToDDSFile(DDS_FLAGS.NONE, path);
                        }
                    }
                }

                imported = ImportDDSFile(texture, path, texture.Type1);
                File.Delete(path);
            }
            else
            {
                imported = ImportDDSFile(texture, path, texture.Type1);
            }

            return imported;
        }

        public bool ImportRGBAFile(Texture texture, string path)
        {
            CreateBackupOfTexture(texture);

            string extension = Path.GetExtension(path);

            if (extension.Equals(".dds"))
            {
                return ImportDDSFile(texture, path, "A8B8G8R8");
            }
            else
            {
                byte[] data;
                int width, height;

                try
                {
                    using (var stream = File.OpenRead(path))
                    {
                        ImageResult image = ImageResult.FromStream(stream, StbImageSharp.ColorComponents.RedGreenBlueAlpha);

                        data = image.Data;
                        width = image.Width;
                        height = image.Height;
                    }
                }
                catch (IOException ex)
                {
                    MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                    return false;
                }

                Bitmap bitmap = BMPImage.DataToBitmap(data, width, height);

                int numOfMipMaps = CalculateNumOfMipMaps(width, height);

                texture.NumOfMipMips = numOfMipMaps;
                texture.MipMapLevelSizes = new int[numOfMipMaps];
                texture.MipMapLevelSizes[0] = data.Length;

                texture.Data = new byte[numOfMipMaps][];
                texture.Data[0] = new byte[data.Length];
                Array.Copy(data, 0, texture.Data[0], 0, data.Length);

                byte[][] mipMaps = GenerateMipMaps(bitmap, numOfMipMaps);

                for (int i = 0; i < mipMaps.Length; i++)
                {
                    byte[] mipMap = mipMaps[i];

                    texture.Data[i + 1] = new byte[mipMap.Length];
                    texture.MipMapLevelSizes[i + 1] = mipMap.Length;

                    Array.Copy(mipMap, 0, texture.Data[i + 1], 0, mipMap.Length);
                }

                texture.FileSize = CalculateFileSize(texture);
            }

            return true;
        }

        public bool ExportDDSFile(Texture texture, string type, string exportPath)
        {
            DDS_HEADER ddsHeader = new DDS_HEADER();

            ddsHeader.magic = new char[] { 'D', 'D', 'S', ' ' };
            ddsHeader.surfaceDesc.size = 124;
            ddsHeader.surfaceDesc.flags = (uint)DDSSurfaceDescFlags.DDSD_CAPS | (uint)DDSSurfaceDescFlags.DDSD_HEIGHT
                | (uint)DDSSurfaceDescFlags.DDSD_WIDTH | (uint)DDSSurfaceDescFlags.DDSD_PIXELFORMAT;

            if (!type.Equals("A8B8G8R8") && !type.Equals("L8"))
            {
                ddsHeader.surfaceDesc.flags |= (uint)DDSSurfaceDescFlags.DDSD_LINEARSIZE;
            }
            else
            {
                ddsHeader.surfaceDesc.flags |= (uint)DDSSurfaceDescFlags.DDSD_PITCH;
            }

            if (texture.NumOfMipMips > 1)
            {
                ddsHeader.surfaceDesc.flags |= (uint)DDSSurfaceDescFlags.DDSD_MIPMAPCOUNT;
            }

            ddsHeader.surfaceDesc.height = (uint)texture.Height;
            ddsHeader.surfaceDesc.width = (uint)texture.Width;

            if (!type.Equals("A8B8G8R8") && !type.Equals("L8"))
            {
                ddsHeader.surfaceDesc.pitchOrLinearSize = (uint)texture.MipMapLevelSizes[0];
            }
            else
            {
                ddsHeader.surfaceDesc.pitchOrLinearSize = (uint)texture.Width * 4;
            }

            ddsHeader.surfaceDesc.depth = 0;
            ddsHeader.surfaceDesc.mipMapCount = (uint)texture.NumOfMipMips;
            ddsHeader.surfaceDesc.reserved1 = new uint[11];

            for (int i = 0; i < ddsHeader.surfaceDesc.reserved1.Length; i++)
            {
                ddsHeader.surfaceDesc.reserved1[i] = 0;
            }

            ddsHeader.surfaceDesc.ddspf.size = 32;

            if (type.Equals("A8B8G8R8"))
            {
                ddsHeader.surfaceDesc.ddspf.flags = (uint)DDSPixelFormatFlags.DDPF_RGB | (uint)DDSPixelFormatFlags.DDPF_ALPHAPIXELS;
                ddsHeader.surfaceDesc.ddspf.rgbBitCount = 32;
                ddsHeader.surfaceDesc.ddspf.dwRBitMask = 0xFF0000;
                ddsHeader.surfaceDesc.ddspf.dwGBitMask = 0xFF00;
                ddsHeader.surfaceDesc.ddspf.dwBBitMask = 0xFF;
                ddsHeader.surfaceDesc.ddspf.dwABitMask = 0xFF000000;
            }
            else if (type.Equals("L8"))
            {
                ddsHeader.surfaceDesc.ddspf.flags = (uint)DDSPixelFormatFlags.DDPF_LUMINANCE;
                ddsHeader.surfaceDesc.ddspf.rgbBitCount = 8;
                ddsHeader.surfaceDesc.ddspf.dwRBitMask = 0xFF;
                ddsHeader.surfaceDesc.ddspf.dwGBitMask = 0;
                ddsHeader.surfaceDesc.ddspf.dwBBitMask = 0;
                ddsHeader.surfaceDesc.ddspf.dwABitMask = 0;
            }
            else
            {
                ddsHeader.surfaceDesc.ddspf.flags = (uint)DDSPixelFormatFlags.DDPF_FOURCC;

                char dxtNum = char.Parse(texture.Type1.Substring(texture.Type1.Length - 1));

                ddsHeader.surfaceDesc.ddspf.fourCC = new char[] { 'D', 'X', 'T', dxtNum };
                ddsHeader.surfaceDesc.ddspf.rgbBitCount = 0;
                ddsHeader.surfaceDesc.ddspf.dwRBitMask = 0;
                ddsHeader.surfaceDesc.ddspf.dwGBitMask = 0;
                ddsHeader.surfaceDesc.ddspf.dwBBitMask = 0;
                ddsHeader.surfaceDesc.ddspf.dwABitMask = 0;
            }

            ddsHeader.surfaceDesc.caps.caps1 = (uint)DDSHeaderCaps.DDSCAPS_TEXTURE;

            if (texture.NumOfMipMips > 1)
            {
                ddsHeader.surfaceDesc.caps.caps1 |= (uint)DDSHeaderCaps.DDSCAPS_COMPLEX | (uint)DDSHeaderCaps.DDSCAPS_MIPMAP;
            }

            ddsHeader.surfaceDesc.caps.caps2 = 0;
            ddsHeader.surfaceDesc.caps.caps3 = 0;
            ddsHeader.surfaceDesc.caps.caps4 = 0;
            ddsHeader.surfaceDesc.reserved2 = 0;

            try
            {
                using (FileStream fs = new FileStream(exportPath, FileMode.Create))
                {
                    using (BinaryWriter binaryWriter = new BinaryWriter(fs))
                    {
                        binaryWriter.Write(ddsHeader.magic);
                        binaryWriter.Write(ddsHeader.surfaceDesc.size);
                        binaryWriter.Write(ddsHeader.surfaceDesc.flags);
                        binaryWriter.Write(ddsHeader.surfaceDesc.height);
                        binaryWriter.Write(ddsHeader.surfaceDesc.width);
                        binaryWriter.Write(ddsHeader.surfaceDesc.pitchOrLinearSize);
                        binaryWriter.Write(ddsHeader.surfaceDesc.depth);
                        binaryWriter.Write(ddsHeader.surfaceDesc.mipMapCount);

                        for (int i = 0; i < ddsHeader.surfaceDesc.reserved1.Length; i++)
                        {
                            binaryWriter.Write(ddsHeader.surfaceDesc.reserved1[i]);
                        }

                        binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.size);
                        binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.flags);

                        if (!type.Equals("A8B8G8R8") && !type.Equals("L8"))
                        {
                            binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.fourCC);
                        }
                        else
                        {
                            binaryWriter.Write(0);
                        }

                        binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.rgbBitCount);
                        binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.dwRBitMask);
                        binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.dwGBitMask);
                        binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.dwBBitMask);
                        binaryWriter.Write(ddsHeader.surfaceDesc.ddspf.dwABitMask);
                        binaryWriter.Write(ddsHeader.surfaceDesc.caps.caps1);
                        binaryWriter.Write(ddsHeader.surfaceDesc.caps.caps2);
                        binaryWriter.Write(ddsHeader.surfaceDesc.caps.caps3);
                        binaryWriter.Write(ddsHeader.surfaceDesc.caps.caps4);
                        binaryWriter.Write(ddsHeader.surfaceDesc.reserved2);

                        for (int i = 0; i < texture.Data.Length; i++)
                        {
                            byte[] data = texture.Data[i];

                            binaryWriter.Write(data);
                        }
                    }
                }
            }
            catch (IOException ex)
            {
                MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                return false;
            }

            return true;
        }

        public bool ExportDXTFile(Texture texture, string exportPath, int index = 0)
        {
            byte[] data = texture.Data[index];

            Texture newTexture = ObjectExtensions.Clone(texture);

            newTexture.NumOfMipMips = 1;
            newTexture.Data = new byte[1][];
            newTexture.Data[0] = data;
            newTexture.MipMapLevelSizes = new int[1];
            newTexture.MipMapLevelSizes[0] = data.Length;

            if (index > 0)
            {
                newTexture.Width /= (int)Math.Pow(2, index);
                newTexture.Height /= (int)Math.Pow(2, index);
            }

            string extension = Path.GetExtension(exportPath);

            if (extension.Equals(".dds"))
            {
                bool exported = ExportDDSFile(newTexture, texture.Type1, exportPath);

                if (!exported)
                {
                    return false;
                }
            }
            else
            {
                try
                {
                    byte[] data2 = ConvertDXTToRGBA(data, newTexture.Width, newTexture.Height, texture.Type1);
                    SaveToImage(data2, newTexture.Width, newTexture.Height, exportPath, extension);
                }
                catch
                {
                    return false;
                }
            }

            return true;
        }

        public bool ExportDXTFile(string exportPath, Texture texture)
        {
            string extension = Path.GetExtension(exportPath);

            ExportDXTFile(texture, exportPath);

            return true;
        }

        public bool ExportDXTAsRGBA(Texture texture, Dictionary<int, string> selectedDimensions, string extension, string exportPath)
        {
            Dictionary<int, string> paths = GetExportPaths(selectedDimensions, texture.Width, texture.Height, extension, exportPath);

            for (int i = 0; i < selectedDimensions.Count; i++)
            {
                int index = selectedDimensions.Keys.ElementAt(i);
                byte[] data = texture.Data[index];

                if (texture.Type1.Equals("U8V8"))
                {
                    data = ConvertU8V8ToBGRA(data);
                }

                string newExportPath = paths.Values.ElementAt(i);
                int width = texture.Width / (int)Math.Pow(2, index);
                int height = texture.Height / (int)Math.Pow(2, index);

                try
                {
                    byte[] data2 = ConvertDXTToRGBA(data, width, height, texture.Type1);
                    SaveToImage(data2, width, height, newExportPath, extension);
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                    return false;
                }
            }

            return true;
        }

        public byte[] ConvertDXTToRGBA(byte[] data, int width, int height, string type)
        {
            byte[] buffer;

            if (type.Equals("DXT1"))
            {
                buffer = DxtUtil.DecompressDxt1(data, width, height);
            }
            else
            {
                buffer = DxtUtil.DecompressDxt3(data, width, height);
            }

            return buffer;
        }

        public bool ExportRGBAFile(Texture texture, string exportPath, int index = 0)
        {
            int width = texture.Width / (int)Math.Pow(2, index);
            int height = texture.Height / (int)Math.Pow(2, index);

            int length = texture.Data[index].Length;
            byte[] data = new byte[length];
            Array.Copy(texture.Data[index], 0, data, 0, length);

            string extension = Path.GetExtension(exportPath);

            if (extension.Equals(".dds"))
            {
                ConvertRGBAToBGRA(data, width, height);

                Texture newTexture = ObjectExtensions.Clone(texture);

                newTexture.NumOfMipMips = 1;
                newTexture.Data = new byte[1][];
                newTexture.Data[0] = data;
                newTexture.MipMapLevelSizes = new int[1];
                newTexture.MipMapLevelSizes[0] = data.Length;

                return ExportDDSFile(newTexture, "A8B8G8R8", exportPath);
            }
            else
            {
                if (extension.Equals(".bmp"))
                {
                    ConvertRGBAToBGRA(data, width, height);
                    Bitmap bmp = BMPImage.DataToBitmap(data, width, height);

                    bmp.Save(exportPath, ImageFormat.Bmp);
                }
                else
                {
                    SaveToImage(data, width, height, exportPath, extension);
                }
            }

            return true;
        }

        public bool ExportRGBAFile(string exportPath, Texture texture)
        {
            string extension = Path.GetExtension(exportPath);
            
            ExportRGBAFile(texture, exportPath);

            return true;
        }

        public void SaveToImage(byte[] data, int width, int height, string exportPath, string extension, StbImageWriteSharp.ColorComponents colorComponents = StbImageWriteSharp.ColorComponents.RedGreenBlueAlpha)
        {
            using (Stream stream = File.OpenWrite(exportPath))
            {
                ImageWriter writer = new ImageWriter();

                switch (extension)
                {
                    case ".tga":
                        writer.WriteTga(data, width, height, colorComponents, stream);
                        break;
                    case ".bmp":
                        writer.WriteBmp(data, width, height, colorComponents, stream);
                        break;
                    case ".png":
                        writer.WritePng(data, width, height, colorComponents, stream);
                        break;
                    default:
                        writer.WriteJpg(data, width, height, colorComponents, stream, 100);
                        break;
                }
            }
        }

        public bool ExportSelectedOptionsToDDS(Texture texture, Dictionary<int, string> selectedDimensions, string type, string exportPath, bool exportOnlyHighestResolution)
        {
            Texture newTexture = ObjectExtensions.Clone(texture);
            newTexture.NumOfMipMips = selectedDimensions.Keys.Count;

            string value = selectedDimensions.Values.ElementAt(0);

            newTexture.Width = Convert.ToInt32(value.Substring(0, value.IndexOf('x')));
            newTexture.Height = Convert.ToInt32(value.Substring(value.IndexOf('x') + 1));

            if (!exportOnlyHighestResolution)
            {
                newTexture.Data = new byte[newTexture.NumOfMipMips][];
                newTexture.MipMapLevelSizes = new int[newTexture.NumOfMipMips];

                for (int i = 0; i < selectedDimensions.Count; i++)
                {
                    int index = selectedDimensions.Keys.ElementAt(i);
                    byte[] data = texture.Data[index];

                    if (texture.Type1.Equals("U8V8"))
                    {
                        data = ConvertU8V8ToBGRA(data);
                    }

                    newTexture.Data[i] = data;
                    newTexture.MipMapLevelSizes[i] = data.Length;
                }
            }
            else
            {
                newTexture.Data = new byte[1][];
                newTexture.MipMapLevelSizes = new int[1];

                int index = selectedDimensions.Keys.ElementAt(0);
                byte[] data = texture.Data[index];

                if (texture.Type1.Equals("U8V8"))
                {
                    data = ConvertU8V8ToBGRA(data);
                }

                newTexture.Data[0] = data;
                newTexture.MipMapLevelSizes[0] = data.Length;
            }

            return ExportDDSFile(newTexture, type, exportPath);
        }

        public Dictionary<int, string> GetExportPaths(Dictionary<int, string> selectedDimensions, int width, int height, string extension, string exportPath)
        {
            Dictionary<int, string> paths = new Dictionary<int, string>();
            string dimensions = width + "x" + height;

            if (selectedDimensions.Count > 0)
            {
                if (!(selectedDimensions.Count == 1 && selectedDimensions.Values.ElementAt(0).Equals(dimensions)))
                {
                    string path = Path.Combine(Path.GetDirectoryName(exportPath), Path.GetFileNameWithoutExtension(exportPath));

                    for (int i = 0; i < selectedDimensions.Count; i++)
                    {
                        string newPath = path + "_" + selectedDimensions.Values.ElementAt(i) + extension;

                        paths.Add(selectedDimensions.Keys.ElementAt(i), newPath);
                    }
                }
                else
                {
                    paths.Add(0, exportPath);
                }
            }

            return paths;
        }

        public byte[][] GetPALNPalette(byte[] palData, int paletteSize)
        {
            byte[][] colors = new byte[paletteSize][];

            for (int i = 0; i < paletteSize; i++)
            {
                colors[i] = new byte[4];

                colors[i][0] = palData[i * 4];
                colors[i][1] = palData[i * 4 + 1];
                colors[i][2] = palData[i * 4 + 2];
                colors[i][3] = palData[i * 4 + 3];
            }

            return colors;
        }

        public byte[][] GetPALNPalette(Texture texture)
        {
            byte[][] colors = new byte[texture.PaletteSize][];

            for (int i = 0; i < texture.PaletteSize; i++)
            {
                colors[i] = new byte[4];

                colors[i][0] = texture.PalData[i * 4];
                colors[i][1] = texture.PalData[i * 4 + 1];
                colors[i][2] = texture.PalData[i * 4 + 2];
                colors[i][3] = texture.PalData[i * 4 + 3];
            }

            return colors;
        }

        public byte[] GetColorsFromPALNRefs(Texture texture, int index = 0)
        {
            int n = 0;
            byte[] data = new byte[texture.Width * texture.Height * 4];
            byte[][] palette = GetPALNPalette(texture);
            byte[] palnData = texture.Data[index];

            for (int i = 0; i < palnData.Length; i++)
            {
                byte[] colorBytes = palette[palnData[i]];

                data[n * 4] = colorBytes[0];
                data[n * 4 + 1] = colorBytes[1];
                data[n * 4 + 2] = colorBytes[2];
                data[n++ * 4 + 3] = colorBytes[3];
            }

            return data;
        }

        public byte[] GetColorsFromPALNRefs(byte[] palnData, int width, int height, int paletteSize)
        {
            int n = 0;
            byte[] data = new byte[width * height * 4];
            byte[][] palette = GetPALNPalette(palnData, paletteSize);

            for (int i = 0; i < palnData.Length; i++)
            {
                byte[] colorBytes = palette[palnData[i]];

                data[n * 4] = colorBytes[0];
                data[n * 4 + 1] = colorBytes[1];
                data[n * 4 + 2] = colorBytes[2];
                data[n++ * 4 + 3] = colorBytes[3];
            }

            return data;
        }

        public byte[] FlipY(byte[] data, int width, int height)
        {
            using (MemoryStream memoryStream = new MemoryStream())
            {
                using (BinaryWriter binaryWriter = new BinaryWriter(memoryStream))
                {
                    using (BinaryReader binaryReader = new BinaryReader(new MemoryStream(data)))
                    {
                        byte[][] result = new byte[height][];

                        for (int i = height; i > 0; i--)
                        {
                            result[i - 1] = binaryReader.ReadBytes(width * 4);
                        }

                        for (int i = 0; i < height; i++)
                        {
                            binaryWriter.Write(result[i]);
                        }
                    }

                    return memoryStream.ToArray();
                }
            }
        }

        public byte[] FlipI8Y(byte[] data, int width, int height)
        {
            using (MemoryStream memoryStream = new MemoryStream())
            {
                using (BinaryWriter binaryWriter = new BinaryWriter(memoryStream))
                {
                    using (BinaryReader binaryReader = new BinaryReader(new MemoryStream(data)))
                    {
                        byte[][] result = new byte[height][];

                        for (int i = height; i > 0; i--)
                        {
                            result[i - 1] = binaryReader.ReadBytes(width);
                        }

                        for (int i = 0; i < height; i++)
                        {
                            binaryWriter.Write(result[i]);
                        }
                    }

                    return memoryStream.ToArray();
                }
            }
        }

        public bool ExportPALNFile(Texture texture, string exportPath, int index = 0)
        {
            int width = texture.Width / (int)Math.Pow(2, index);
            int height = texture.Height / (int)Math.Pow(2, index);

            byte[] data = GetColorsFromPALNRefs(texture, index);

            string extension = Path.GetExtension(exportPath);

            if (extension.Equals(".dds"))
            {
                Texture newTexture = ObjectExtensions.Clone(texture);

                newTexture.NumOfMipMips = 1;
                newTexture.Data = new byte[1][];
                newTexture.Data[0] = data;
                newTexture.MipMapLevelSizes = new int[1];
                newTexture.MipMapLevelSizes[0] = data.Length;

                return ExportDDSFile(newTexture, "A8B8G8R8", exportPath);
            }
            else if (extension.Equals(".tga"))
            {
                byte[] buffer = FlipY(data, width, height);

                TGAImage.SaveToTGA32(buffer, width, height, exportPath);
            }
            else if (extension.Equals(".bmp"))
            {
                byte[] data2 = new byte[data.Length];
                Array.Copy(data, 0, data2, 0, data.Length);

                Bitmap bmp = BMPImage.DataToBitmap(data, width, height);

                bmp.Save(exportPath, ImageFormat.Bmp);
            }
            else
            {
                SaveToImage(data, width, height, exportPath, extension);
            }

            return true;
        }

        public bool ExportPALNFile(string exportPath, Texture texture)
        {
            string extension = Path.GetExtension(exportPath);
            
            ExportPALNFile(texture, exportPath);

            return true;
        }

        public bool ExportI8File(Texture texture, string exportPath, int index = 0)
        {
            int width = texture.Width / (int)Math.Pow(2, index);
            int height = texture.Height / (int)Math.Pow(2, index);

            byte[] i8Data = texture.Data[index];
            string extension = Path.GetExtension(exportPath);

            if (extension.Equals(".dds"))
            {
                Texture newTexture = ObjectExtensions.Clone(texture);

                newTexture.NumOfMipMips = 1;
                newTexture.Data = new byte[1][];
                newTexture.Data[0] = i8Data;
                newTexture.MipMapLevelSizes = new int[1];
                newTexture.MipMapLevelSizes[0] = i8Data.Length;

                return ExportDDSFile(newTexture, "L8", exportPath);
            }

            if (extension.Equals(".tga"))
            {
                byte[] buffer = FlipI8Y(i8Data, width, height);

                TGAImage.SaveToTGA8(buffer, width, height, 256, false, exportPath);
            }
            else if (extension.Equals(".bmp"))
            {
                byte[] data = new byte[i8Data.Length];
                Array.Copy(i8Data, 0, data, 0, data.Length);

                Bitmap bmp = BMPImage.DataToBitmap(data, width, height, PixelFormat.Format8bppIndexed);

                bmp.Save(exportPath, ImageFormat.Bmp);
            }
            else
            {
                SaveToImage(texture.Data[0], width, height, exportPath, extension, StbImageWriteSharp.ColorComponents.Grey);
            }

            return true;
        }

        public bool ExportI8File(string exportPath, Texture texture)
        {
            string extension = Path.GetExtension(exportPath);
            
            ExportI8File(texture, exportPath);

            return true;
        }

        public bool ExportU8V8File(Texture texture, string exportPath, int index = 0)
        {
            int width = texture.Width / (int)Math.Pow(2, index);
            int height = texture.Height / (int)Math.Pow(2, index);

            byte[] u8v8Data = texture.Data[index];
            byte[] colors = ConvertU8V8ToBGRA(u8v8Data);

            string extension = Path.GetExtension(exportPath);

            if (extension.Equals(".dds"))
            {
                Texture newTexture = ObjectExtensions.Clone(texture);

                newTexture.NumOfMipMips = 1;
                newTexture.Data = new byte[1][];
                newTexture.Data[0] = colors;
                newTexture.MipMapLevelSizes = new int[1];
                newTexture.MipMapLevelSizes[0] = colors.Length;

                return ExportDDSFile(texture, "A8B8G8R8", exportPath);
            }
            else if (extension.Equals(".tga"))
            {
                byte[] buffer = FlipY(colors, width, height);

                TGAImage.SaveToTGA32(buffer, width, height, exportPath);
            }
            else if (extension.Equals(".bmp"))
            {
                byte[] data = new byte[u8v8Data.Length];
                Array.Copy(u8v8Data, 0, data, 0, data.Length);

                Bitmap bmp = BMPImage.DataToBitmap(data, width, height);

                bmp.Save(exportPath, ImageFormat.Bmp);
            }
            else
            {
                SaveToImage(colors, width, height, exportPath, extension);
            }

            return true;
        }

        public bool ExportU8V8File(string exportPath, Texture texture)
        {
            string extension = Path.GetExtension(exportPath);
            
            ExportU8V8File(texture, exportPath);

            return true;
        }

        public byte[] ConvertU8V8ToRGBA(byte[] data)
        {
            byte[] output;

            using (BinaryReader binaryReader = new BinaryReader(new MemoryStream(data)))
            {
                using (MemoryStream memoryStream = new MemoryStream())
                {
                    using (BinaryWriter binaryWriter = new BinaryWriter(memoryStream))
                    {
                        for (int i = 0; i < data.Length / 2; i++)
                        {
                            byte red = binaryReader.ReadByte();
                            byte green = binaryReader.ReadByte();

                            binaryWriter.Write(new byte[] { red, green, 255, 255 });
                        }

                        output = memoryStream.ToArray();
                    }
                }
            }

            return output;
        }

        public byte[] ConvertU8V8ToBGRA(byte[] data)
        {
            byte[] output;

            using (BinaryReader binaryReader = new BinaryReader(new MemoryStream(data)))
            {
                using (MemoryStream memoryStream = new MemoryStream())
                {
                    using (BinaryWriter binaryWriter = new BinaryWriter(memoryStream))
                    {
                        for (int i = 0; i < data.Length / 2; i++)
                        {
                            byte red = binaryReader.ReadByte();
                            byte green = binaryReader.ReadByte();

                            binaryWriter.Write(new byte[] { 255, green, red, 255 });
                        }

                        output = memoryStream.ToArray();
                    }
                }
            }

            return output;
        }

        public byte[][] ConvertU8V8ToBGRA(Texture texture)
        {
            byte[][] data = new byte[texture.Data.Length][];

            for (int i = 0; i < texture.Data.Length; i++)
            {
                using (BinaryReader binaryReader = new BinaryReader(new MemoryStream(texture.Data[i])))
                {
                    using (MemoryStream memoryStream = new MemoryStream())
                    {
                        using (BinaryWriter binaryWriter = new BinaryWriter(memoryStream))
                        {
                            for (int j = 0; j < texture.Data[i].Length / 2; j++)
                            {
                                byte red = binaryReader.ReadByte();
                                byte green = binaryReader.ReadByte();

                                binaryWriter.Write(new byte[] { 255, green, red, 255 });
                            }

                            data[i] = memoryStream.ToArray();
                        }
                    }
                }
            }

            return data;
        }

        public byte[] ColorsToPALNRefs(byte[] data)
        {
            List<Color> colors = new List<Color>();

            using (MemoryStream memoryStream = new MemoryStream())
            {
                using (BinaryReader binaryReader = new BinaryReader(new MemoryStream(data)))
                {
                    for (int i = 0; i < data.Length / 4; i++)
                    {
                        byte[] colorBytes = binaryReader.ReadBytes(4);

                        Color color = Color.FromArgb(colorBytes[3], colorBytes[0], colorBytes[1], colorBytes[2]);
                        colors.Add(color);
                    }
                }
            }

            colors = colors.Distinct().OrderBy(c => c.A).ToList();

            while (colors.Count < 16)
            {
                Color color = Color.FromArgb(255, 255, 255, 255);
                colors.Add(color);
            }

            byte[] buffer = new byte[data.Length];
            int n = 0;

            using (MemoryStream memoryStream = new MemoryStream())
            {
                using (BinaryReader binaryReader = new BinaryReader(new MemoryStream(data)))
                {
                    for (int i = 0; i < data.Length / 4; i++)
                    {
                        byte[] colorBytes = binaryReader.ReadBytes(4);

                        Color color = Color.FromArgb(colorBytes[3], colorBytes[0], colorBytes[1], colorBytes[2]);
                        int colorRef = colors.FindIndex(c => c == color);

                        buffer[n++] = (byte)colorRef;
                    }
                }
            }

            return buffer;
        }

        public byte[] ColorsToPALNRefs(Bitmap bitmap)
        {
            BMPImage bmpImage = new BMPImage(bitmap);
            List<Color> colors = new List<Color>();

            bmpImage.LockBits();

            for (int x = 0; x < bitmap.Width; x++)
            {
                for (int y = 0; y < bitmap.Height; y++)
                {
                    Color color = bmpImage.GetPixel(x, y);
                    colors.Add(color);
                }
            }

            bmpImage.UnlockBits();

            colors = colors.Distinct().OrderBy(c => c.A).ToList();

            while (colors.Count < 16)
            {
                Color color = Color.FromArgb(255, 255, 255, 255);
                colors.Add(color);
            }

            byte[] buffer = new byte[bitmap.Width * bitmap.Height];

            bmpImage.LockBits();

            for (int y = 0; y < bitmap.Height; y++)
            {
                for (int x = 0; x < bitmap.Width; x++)
                {
                    Color color = bmpImage.GetPixel(x, y);
                    int colorRef = colors.FindIndex(c => c == color);

                    buffer[y * bitmap.Width + x] = (byte)colorRef;
                }
            }

            bmpImage.UnlockBits();

            return buffer;
        }

        public bool ImportPALNFile(Texture texture, string path)
        {
            CreateBackupOfTexture(texture);

            string extension = Path.GetExtension(path);

            if (extension.Equals(".dds"))
            {
                return ImportDDSFile(texture, path, "A8B8G8R8");
            }

            Bitmap bitmap;
            byte[] data;
            int width, height;

            if (extension.Equals(".tga"))
            {
                bitmap = Paloma.TargaImage.LoadTargaImage(path);

                width = bitmap.Width;
                height = bitmap.Height;
            }
            else
            {
                try
                {
                    using (var stream = File.OpenRead(path))
                    {
                        ImageResult image = ImageResult.FromStream(stream, StbImageSharp.ColorComponents.RedGreenBlueAlpha);

                        data = image.Data;
                        width = image.Width;
                        height = image.Height;
                    }
                }
                catch (IOException ex)
                {
                    MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                    return false;
                }

                bitmap = BMPImage.DataToBitmap(data, width, height);
            }

            data = ColorsToPALNRefs(bitmap);

            int numOfMipMaps = CalculateNumOfMipMaps(width, height);

            texture.NumOfMipMips = numOfMipMaps;
            texture.MipMapLevelSizes = new int[numOfMipMaps];
            texture.MipMapLevelSizes[0] = data.Length;

            texture.Data = new byte[numOfMipMaps][];
            texture.Data[0] = new byte[data.Length];
            Array.Copy(data, 0, texture.Data[0], 0, data.Length);

            byte[][] mipMaps = GenerateMipMaps(bitmap, numOfMipMaps);

            for (int i = 0; i < mipMaps.Length; i++)
            {
                byte[] mipMap = mipMaps[i];

                texture.Data[i + 1] = new byte[mipMap.Length];
                texture.MipMapLevelSizes[i + 1] = mipMap.Length;

                Array.Copy(mipMap, 0, texture.Data[i + 1], 0, mipMap.Length);
            }

            texture.FileSize = CalculateFileSize(texture);

            return true;
        }

        public byte[][] GetGrayScalePalette()
        {
            byte[][] palette = new byte[256][];

            for (int y = 0; y <= 255; y++)
            {
                palette[y] = new byte[4];

                for (int i = 0; i < 3; i++)
                {
                    palette[y][i] = (byte)y;
                }

                palette[y][3] = 0;
            }

            return palette;
        }

        public byte[] GetI8ColorReferences(Bitmap bitmap)
        {
            BMPImage bmpImage = new BMPImage(bitmap);
            byte[] buffer = new byte[bitmap.Width * bitmap.Height];
            byte[][] palette = GetGrayScalePalette();

            bmpImage.LockBits();

            for (int y = 0; y < bitmap.Height; y++)
            {
                for (int x = 0; x < bitmap.Width; x++)
                {
                    Color color = bmpImage.GetPixel(x, y);
                    int colorRef = 0;

                    for (int i = 0; i < palette.Length; i++)
                    {
                        byte[] colorBytes = palette[i];
                        Color color2 = Color.FromArgb(255, colorBytes[0], colorBytes[0], colorBytes[0]);

                        if (color == color2)
                        {
                            colorRef = i;
                            break;
                        }
                    }

                    buffer[y * bitmap.Width + x] = (byte)colorRef;
                }
            }

            bmpImage.UnlockBits();

            return buffer;
        }

        public bool ImportI8File(Texture texture, string path)
        {
            CreateBackupOfTexture(texture);

            string extension = Path.GetExtension(path);

            if (extension.Equals(".dds"))
            {
                return ImportDDSFile(texture, path, "L8");
            }

            Bitmap bitmap;
            byte[] data;
            int width, height;

            if (extension.Equals(".tga"))
            {
                bitmap = Paloma.TargaImage.LoadTargaImage(path);
            }
            else
            {
                try
                {
                    using (var stream = File.OpenRead(path))
                    {
                        ImageResult image = ImageResult.FromStream(stream, StbImageSharp.ColorComponents.RedGreenBlueAlpha);

                        data = image.Data;
                        width = image.Width;
                        height = image.Height;
                    }
                }
                catch (IOException ex)
                {
                    MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                    return false;
                }

                bitmap = BMPImage.DataToBitmap(data, width, height);
            }

            byte[] buffer = GetI8ColorReferences(bitmap);

            int numOfMipMaps = CalculateNumOfMipMaps(bitmap.Width, bitmap.Height);

            texture.NumOfMipMips = numOfMipMaps;
            texture.MipMapLevelSizes = new int[numOfMipMaps];
            texture.MipMapLevelSizes[0] = buffer.Length;

            texture.Data = new byte[numOfMipMaps][];
            texture.Data[0] = new byte[buffer.Length];
            Array.Copy(buffer, 0, texture.Data[0], 0, buffer.Length);

            byte[][] mipMaps = GenerateMipMaps(bitmap, numOfMipMaps);

            for (int i = 0; i < mipMaps.Length; i++)
            {
                byte[] mipMap = mipMaps[i];

                texture.Data[i] = new byte[mipMap.Length];
                texture.MipMapLevelSizes[i] = mipMap.Length;

                Array.Copy(mipMap, 0, texture.Data[i], 0, mipMap.Length);
            }

            texture.FileSize = CalculateFileSize(texture);

            return true;
        }

        public bool ImportU8V8File(Texture texture, string path)
        {
            CreateBackupOfTexture(texture);

            string extension = Path.GetExtension(path);

            if (extension.Equals(".dds"))
            {
                return ImportDDSFile(texture, path, "A8B8G8R8");
            }

            Bitmap bitmap;
            byte[] buffer;
            int width, height;

            if (extension.Equals(".tga"))
            {
                bitmap = Paloma.TargaImage.LoadTargaImage(path);

                width = bitmap.Width;
                height = bitmap.Height;
            }
            else
            {
                try
                {
                    using (var stream = File.OpenRead(path))
                    {
                        ImageResult image = ImageResult.FromStream(stream, StbImageSharp.ColorComponents.RedGreenBlueAlpha);

                        buffer = image.Data;
                        width = image.Width;
                        height = image.Height;
                    }
                }
                catch (IOException ex)
                {
                    MessageBox.Show(ex.Message, "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);

                    return false;
                }

                bitmap = BMPImage.DataToBitmap(buffer, width, height);
            }

            BMPImage bmpImage = new BMPImage(bitmap);
            List<byte> data = new List<byte>();

            bmpImage.LockBits();

            for (int x = 0; x < bitmap.Width; x++)
            {
                for (int y = 0; y < bitmap.Height; y++)
                {
                    Color color = bmpImage.GetPixel(x, y);

                    data.Add(color.R);
                    data.Add(color.G);
                }
            }

            bmpImage.UnlockBits();

            int numOfMipMaps = CalculateNumOfMipMaps(bitmap.Width, bitmap.Height);

            texture.NumOfMipMips = numOfMipMaps;
            texture.MipMapLevelSizes = new int[numOfMipMaps];
            texture.MipMapLevelSizes[0] = data.Count;

            texture.Data = new byte[numOfMipMaps][];
            texture.Data[0] = new byte[data.Count];
            Array.Copy(data.ToArray(), 0, texture.Data[0], 0, data.Count);

            int n = 0;

            while (n < numOfMipMaps - 1)
            {
                width /= 2;
                height /= 2;

                Bitmap resized = new Bitmap(bitmap, new Size(width, height));
                bmpImage = new BMPImage(resized);

                List<byte> data2 = new List<byte>();

                bmpImage.LockBits();

                for (int x = 0; x < resized.Width; x++)
                {
                    for (int y = 0; y < resized.Height; y++)
                    {
                        Color color = bmpImage.GetPixel(x, y);

                        data2.Add(color.R);
                        data2.Add(color.G);
                    }
                }

                bmpImage.UnlockBits();

                texture.Data[n] = new byte[data2.Count];
                Array.Copy(data2.ToArray(), 0, texture.Data[n++], 0, data2.Count);
            }

            return true;
        }

        public string ShowSaveFileDialog(string title, string fileName, string filter)
        {
            SaveFileDialog saveFileDialog = new SaveFileDialog();

            saveFileDialog.Title = title;
            saveFileDialog.FileName = fileName;
            saveFileDialog.Filter = filter;

            if (saveFileDialog.ShowDialog() == DialogResult.OK)
            {
                return saveFileDialog.FileName;
            }

            return null;
        }

        public bool ExportFile(string dir, string file, Int32 texture_id)
        {
            Texture texture = textures.Where(t => t.Index == texture_id).FirstOrDefault();

            if (texture == null)
                return false;

            Directory.CreateDirectory(dir);

            string exportPath = dir + "/" + file;

            //Console.WriteLine(texture_id.ToString() + " " + texture.FileName);

            string type = texture.Type1;

            bool exported = false;

            switch (type)
            {
                case "DXT1":
                    exported = ExportDXTFile(exportPath, texture);
                    break;
                case "DXT3":
                    exported = ExportDXTFile(exportPath, texture);
                    break;
                case "RGBA":
                    exported = ExportRGBAFile(exportPath, texture);
                    break;
                case "PALN":
                    exported = ExportPALNFile(exportPath, texture);
                    break;
                case "I8  ":
                    exported = ExportI8File(exportPath, texture);
                    break;
                case "U8V8":
                    exported = ExportU8V8File(exportPath, texture);
                    break;
                default:
                    MessageBox.Show("Unknown texture type.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    break;
            }

            return exported;
        }

        public void ExportAllFiles()
        {
            /*
            toolStripStatusLabel1.Text = "Exporting all textures...";

            using (ExportAllTextures exportAllTextures = new ExportAllTextures(this))
            {
                exportAllTextures.Textures = textures;
                DialogResult dialogResult = exportAllTextures.ShowDialog();

                if (dialogResult == DialogResult.OK)
                {
                    Dictionary<int, Option> options = exportAllTextures.ExportOptions;

                    var watch = Stopwatch.StartNew();

                    foreach (KeyValuePair<int, Option> option in options)
                    {
                        Texture texture = textures[option.Key];

                        Invoke(new Action(() => smoothProgressBar1.Value += (float)100 / textures.Count));
                        Invoke(new Action(() => lblProgress.Text = Math.Round(smoothProgressBar1.Value, 2).ToString() + "%"));

                        string remainingTime = "Remaining Time: " + GetRemainingTime(watch, option.Key, options.Count).ToString("mm':'ss");
                        Invoke(new Action(() => lblRemainingTime.Text = remainingTime));

                        foreach (var entry in option.Value.Extensions)
                        {
                            string extension = entry.Key;
                            string exportPath = Path.Combine(exportAllTextures.ExportPath, texture.GetFileName(extension));

                            if (extension.Equals(".dds"))
                            {
                                if (option.Value.ExportAsSingleFile)
                                {
                                    ExportSelectedOptionsToDDS(texture, entry.Value, texture.Type1, exportPath,
                                        exportAllTextures.ExportOnlyHighestResolution);
                                }
                                else
                                {
                                    Dictionary<int, string> paths;

                                    if (!exportAllTextures.ExportOnlyHighestResolution)
                                    {
                                        paths = GetExportPaths(entry.Value, texture.Width, texture.Height, extension, exportPath);
                                    }
                                    else
                                    {
                                        paths = GetExportPaths(null, texture.Width, texture.Height, extension, exportPath);
                                    }

                                    foreach (KeyValuePair<int, string> entry2 in paths)
                                    {
                                        switch (texture.Type1)
                                        {
                                            case "DXT1":
                                                ExportDXTFile(texture, entry2.Value, entry2.Key);
                                                break;
                                            case "DXT3":
                                                ExportDXTFile(texture, entry2.Value, entry2.Key);
                                                break;
                                            case "RGBA":
                                                ExportRGBAFile(texture, entry2.Value, entry2.Key);
                                                break;
                                            case "PALN":
                                                ExportPALNFile(texture, entry2.Value, entry2.Key);
                                                break;
                                            case "I8  ":
                                                ExportI8File(texture, entry2.Value, entry2.Key);
                                                break;
                                            case "U8V8":
                                                ExportU8V8File(texture, entry2.Value, entry2.Key);
                                                break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                Dictionary<int, string> paths;

                                if (!exportAllTextures.ExportOnlyHighestResolution)
                                {
                                    paths = GetExportPaths(entry.Value, texture.Width, texture.Height, extension, exportPath);
                                }
                                else
                                {
                                    paths = GetExportPaths(null, texture.Width, texture.Height, extension, exportPath);
                                }

                                foreach (KeyValuePair<int, string> entry2 in paths)
                                {
                                    switch (texture.Type1)
                                    {
                                        case "DXT1":
                                            ExportDXTFile(texture, entry2.Value, entry2.Key);
                                            break;
                                        case "DXT3":
                                            ExportDXTFile(texture, entry2.Value, entry2.Key);
                                            break;
                                        case "RGBA":
                                            ExportRGBAFile(texture, entry2.Value, entry2.Key);
                                            break;
                                        case "PALN":
                                            ExportPALNFile(texture, entry2.Value, entry2.Key);
                                            break;
                                        case "I8  ":
                                            ExportI8File(texture, entry2.Value, entry2.Key);
                                            break;
                                        case "U8V8":
                                            ExportU8V8File(texture, entry2.Value, entry2.Key);
                                            break;
                                    }
                                }
                            }
                        }
                    }

                    watch.Stop();
                }
            }
            */
        }

        public string ReverseTypeText(string type)
        {
            char[] charArray = type.ToCharArray();
            Array.Reverse(charArray);

            return new string(charArray);
        }

        public void Align(BinaryWriter binaryWriter, int alignment, byte padding = 0x00)
        {
            long position = binaryWriter.BaseStream.Position;

            if (position % alignment == 0)
            {
                return;
            }

            long number = alignment - position % alignment;

            for (long i = 0; i < number; i++)
            {
                binaryWriter.Write(padding);
            }
        }

        public string GetOutputPath()
        {
            string outputPath = Path.Combine(Path.GetDirectoryName(Application.ExecutablePath), "Temp");

            if (!Directory.Exists(outputPath))
            {
                Directory.CreateDirectory(outputPath);
            }

            return outputPath;
        }

        public Bitmap ConvertDXTToBitmap(byte[] textureData, int width, int height, string type)
        {
            byte[] data = ConvertDXTToRGBA(textureData, width, height, type);

            byte[] data2 = new byte[data.Length];
            Array.Copy(data, 0, data2, 0, data.Length);

            ConvertRGBAToBGRA(data2, width, height);

            return BMPImage.DataToBitmap(data2, width, height);
        }

        public Bitmap ConvertRGBAToBitmap(byte[] textureData, int width, int height)
        {
            int length = textureData.Length;
            byte[] data = new byte[length];
            Array.Copy(textureData, 0, data, 0, length);

            ConvertRGBAToBGRA(data, width, height);

            return BMPImage.DataToBitmap(data, width, height);
        }

        public Bitmap ConvertPALNToBitmap(byte[] textureData, int width, int height, int paletteSize)
        {
            byte[] data = GetColorsFromPALNRefs(textureData, width, height, paletteSize);
            ConvertRGBAToBGRA(data, width, height);

            return BMPImage.DataToBitmap(data, width, height);
        }

        public Bitmap ConvertI8ToBitmap(byte[] textureData, int width, int height)
        {
            return BMPImage.DataToBitmap(textureData, width, height, PixelFormat.Format8bppIndexed);
        }

        public Bitmap ConvertU8V8ToBitmap(byte[] textureData, int width, int height)
        {
            byte[] colors = ConvertU8V8ToBGRA(textureData);

            return BMPImage.DataToBitmap(colors, width, height);
        }

        public Bitmap ResizeTexture(Texture texture, int width, int height)
        {
            Bitmap bitmap = null;

            switch (texture.Type1)
            {
                case "DXT1":
                    bitmap = ConvertDXTToBitmap(texture.Data[0], texture.Width, texture.Height, texture.Type1);
                    break;
                case "DXT3":
                    bitmap = ConvertDXTToBitmap(texture.Data[0], texture.Width, texture.Height, texture.Type1);
                    break;
                case "RGBA":
                    bitmap = ConvertRGBAToBitmap(texture.Data[0], texture.Width, texture.Height);
                    break;
                case "PALN":
                    bitmap = ConvertPALNToBitmap(texture.Data[0], texture.Width, texture.Height, texture.PaletteSize);
                    break;
                case "I8  ":
                    bitmap = ConvertI8ToBitmap(texture.Data[0], texture.Width, texture.Height);
                    break;
                case "U8V8":
                    bitmap = ConvertU8V8ToBitmap(texture.Data[0], texture.Width, texture.Height);
                    break;
                default:
                    MessageBox.Show("Unknown texture type.", "Glacier TEX Editor", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    break;
            }

            Bitmap resized = new Bitmap(bitmap, new Size(width, height));

            return resized;
        }

        public void ConvertRGBAToBGRA(byte[] data, int width, int height)
        {
            for (int i = 0; i < width * height; i++)
            {
                byte r = data[i * 4];
                byte g = data[i * 4 + 1];
                byte b = data[i * 4 + 2];
                byte a = data[i * 4 + 3];

                data[i * 4] = b;
                data[i * 4 + 1] = g;
                data[i * 4 + 2] = r;
                data[i * 4 + 3] = a;
            }
        }

        public void ConvertBGRAToRGBA(byte[] data, int width, int height)
        {
            for (int i = 0; i < width * height; i++)
            {
                byte b = data[i * 4];
                byte g = data[i * 4 + 1];
                byte r = data[i * 4 + 2];
                byte a = data[i * 4 + 3];

                data[i * 4] = r;
                data[i * 4 + 1] = g;
                data[i * 4 + 2] = b;
                data[i * 4 + 3] = a;
            }
        }

        public string ShortenPath(string path)
        {
            int index = path.IndexOf('\\', path.Length / 5);
            string str1 = path.Substring(index + 1);
            string str2 = str1.Substring(0, str1.IndexOf('\\', 1));

            return path.Replace(str2, "...");
        }

        public byte[] GetDDSMainImageData(string path, string type, out int width, out int height)
        {
            using (FileStream fileStream = new FileStream(path, FileMode.Open, FileAccess.Read))
            {
                using (BinaryReader binaryReader = new BinaryReader(fileStream))
                {
                    for (int i = 0; i < 3; i++)
                    {
                        binaryReader.ReadInt32();
                    }

                    height = binaryReader.ReadInt32();
                    width = binaryReader.ReadInt32();

                    for (int i = 0; i < 27; i++)
                    {
                        binaryReader.ReadInt32();
                    }

                    int mainImageSize = CalculateMainImageSize(type, width, height);
                    byte[] data = binaryReader.ReadBytes(mainImageSize);

                    return data;
                }
            }
        }
    }
}