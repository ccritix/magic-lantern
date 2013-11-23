﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using uint8_t = System.Byte;
using uint16_t = System.UInt16;
using uint32_t = System.UInt32;
using uint64_t = System.UInt64;
using int8_t = System.Byte;
using int16_t = System.Int16;
using int32_t = System.Int32;
using int64_t = System.Int64;
using System.Runtime.InteropServices;
using System.Collections;

namespace mlv_view_sharp
{
    /* file footer data */
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    struct lv_rec_file_footer_t
    {
        [MarshalAsAttribute(UnmanagedType.ByValTStr, SizeConst = 4)]
        public string magic;
        public int16_t xRes;
        public int16_t yRes;
        public int32_t frameSize;
        public int32_t frameCount;
        public int32_t frameSkip;
        public int32_t sourceFpsx1000;
        public int32_t reserved3;
        public int32_t reserved4;
        public MLVTypes.raw_info raw_info;
    }

    internal static class RAWHelper
    {
        internal static T ReadStruct<T>(this byte[] buffer)
            where T : struct
        {
            GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
            T result = (T)Marshal.PtrToStructure(handle.AddrOfPinnedObject(), typeof(T));
            handle.Free();
            return result;
        }
    }

    public class RAWReader : MLVReader
    {
        private MLVTypes.mlv_file_hdr_t FileHeader;
        private MLVTypes.mlv_rawi_hdr_t RawiHeader;
        private MLVTypes.mlv_vidf_hdr_t VidfHeader;
        private lv_rec_file_footer_t Footer;
        private byte[] FrameBuffer;

        public RAWReader()
        {
        }

        public RAWReader(string fileName, MLVBlockHandler handler)
        {
            Handler = handler;

            /* ensure that we load the main file first */
            fileName = fileName.Substring(0, fileName.Length - 2) + "AW";

            /* load files and build internal index */
            OpenFiles(fileName);

            if (FileNames == null)
            {
                throw new ArgumentException();
            }

            RawiHeader = ReadFooter();
            BuildIndex();

            /* fill with dummy values */
            FileHeader.audioClass = 0;
            FileHeader.videoClass = 1;

            Handler("MLVI", FileHeader, FrameBuffer, 0, 0);
            Handler("RAWI", RawiHeader, FrameBuffer, 0, 0);
        }

        private void BuildIndex()
        {
            ArrayList list = new ArrayList();
            long currentFileOffset = 0;
            int fileNum = 0;
            int block = 0;

            while (fileNum < Reader.Length)
            {
                /* create xref entry */
                xrefEntry xref = new xrefEntry();

                xref.fileNumber = fileNum;
                xref.position = currentFileOffset;
                xref.timestamp = (ulong)(block * (1000000000.0d / Footer.sourceFpsx1000));

                list.Add(xref);

                /* increment position */
                block++;
                currentFileOffset += Footer.frameSize;

                /* if the read goes beyond the file end, go to next file */
                if (currentFileOffset > Reader[fileNum].BaseStream.Length)
                {
                    currentFileOffset -= Reader[fileNum].BaseStream.Length;
                    fileNum++;
                }

                /* the last block isnt a frame, but the footer. so skip that one. */
                if ((fileNum == Reader.Length - 1) && (Reader[fileNum].BaseStream.Length - currentFileOffset < Footer.frameSize))
                {
                    break;
                }
            }
            BlockIndex = ((xrefEntry[])list.ToArray(typeof(xrefEntry))).OrderBy(x => x.timestamp).ToArray<xrefEntry>();
        }

        private MLVTypes.mlv_rawi_hdr_t ReadFooter()
        {
            MLVTypes.mlv_rawi_hdr_t rawi = new MLVTypes.mlv_rawi_hdr_t();
            int fileNum = FileNames.Length - 1;
            int headerSize = Marshal.SizeOf(typeof(lv_rec_file_footer_t));
            byte[] buf = new byte[headerSize];

            Reader[fileNum].BaseStream.Position = Reader[fileNum].BaseStream.Length - headerSize;

            if (Reader[fileNum].Read(buf, 0, headerSize) != headerSize)
            {
                throw new ArgumentException();
            }

            Reader[fileNum].BaseStream.Position = 0;

            string type = Encoding.UTF8.GetString(buf, 0, 4);
            if (type != "RAWM")
            {
                throw new ArgumentException();
            }

            Footer = RAWHelper.ReadStruct<lv_rec_file_footer_t>(buf);

            /* now forge necessary information */
            rawi.raw_info = Footer.raw_info;
            rawi.xRes = (ushort)Footer.xRes;
            rawi.yRes = (ushort)Footer.yRes;

            FrameBuffer = new byte[Footer.frameSize];

            return rawi;
        }


        public override bool ReadBlock()
        {
            if (Reader == null)
            {
                return false;
            }

            int fileNum = BlockIndex[CurrentBlockNumber].fileNumber;
            long filePos = BlockIndex[CurrentBlockNumber].position;

            /* seek to current block pos */
            Reader[fileNum].BaseStream.Position = filePos;

            int read = Reader[fileNum].Read(FrameBuffer, 0, Footer.frameSize);
            if (read != Footer.frameSize)
            {
                if (fileNum >= Reader.Length || read < 0)
                {
                    return false;
                }

                /* second part from the frame is in the next file */
                fileNum++;
                Reader[fileNum].BaseStream.Position = 0;
                if (Reader[fileNum].Read(FrameBuffer, read, Footer.frameSize - read) != Footer.frameSize - read)
                {
                    return false;
                }
            }

            VidfHeader.frameSpace = 0;

            Handler("VIDF", VidfHeader, FrameBuffer, 0, Footer.frameSize);

            LastType = "VIDF";

            return true;
        }
    }
}
