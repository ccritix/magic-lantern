/*
 * Copyright (C) 2013 Magic Lantern Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/* system includes */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/* project includes */
#include "../lv_rec/lv_rec.h"
#include "../../src/raw.h"
#include "mlv.h"

/* helper macros */
#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


void show_usage(char *executable)
{
    fprintf(stderr, "Usage: %s [-o outputfile] [-c] [-z] [-r] <inputfile>\n", executable);
}

int main (int argc, char *argv[])
{
    char *input_filename = NULL;
    char *output_filename = NULL;
    int mlv_output = 0;
    int raw_output = 0;
    int clean_output = 0;
    int compress_output = 0;
    int verbose = 0;
    int blocks_processed = 0;
    char opt = ' ';
    
    
    /* options we parse:
           -o <file>  # output filename
           -r         # write legacy raw file
           -c         # clean output data (for MLV output)
           -z         # compress output data (for MLV output)
      */
    while ((opt = getopt(argc, argv, "vrzco:")) != -1) 
    {
        switch (opt)
        {
            case 'v':
                verbose = 1;
                break;
                
            case 'r':
                raw_output = 1;
                break;
                
            case 'z':
                compress_output = 1;
                break;
                
            case 'c':
                clean_output = 1;
                break;
                
            case 'o':
                output_filename = strdup(optarg);
                break;
                
            default:
                show_usage(argv[0]);
                return 0;
        }
    }   
    
    if(optind >= argc)
    {
        fprintf(stderr, "Error: Missing input filename\n");
        show_usage(argv[0]);
        return 0;
    }
    
    /* get first file */
    input_filename = argv[optind];
    
    printf("Operating mode:\n"); 
    printf(" - Input MLV file: '%s'\n", input_filename); 
    
    if(verbose)
    {
        printf(" - Verbose messages\n"); 
    }
    
    /* display and set/unset variables according to parameters to have a consistent state */
    if(output_filename)
    {
        if(raw_output)
        {
            compress_output = 0;
            clean_output = 0;
            mlv_output = 0;
            printf(" - Convert to legacy RAW\n"); 
        }
        else
        {
            mlv_output = 1;
            printf(" - Rewrite MLV\n"); 
            if(compress_output)
            {
                printf(" - Compress frame data (not implemented yet)\n"); 
            }
            if(clean_output)
            {
                printf(" - Clean output file\n"); 
            }
        }
        
        printf(" - Output into '%s'\n", output_filename); 
    }
    else
    {
        /* those dont make sense then */
        raw_output = 0;
        compress_output = 0;
        clean_output = 0;
        
        printf(" - Verify file structure\n"); 
        if(verbose)
        {
            printf(" - Dump all block information\n"); 
        }
    }


    /* start processing */
    lv_rec_file_footer_t lv_rec_footer;
    mlv_file_hdr_t main_header;
    
    uint32_t seq_number = 0;
    uint32_t frame_buffer_size = 32*1024*1024;
    char *frame_buffer = NULL;
    FILE *out_file = NULL;
    FILE *in_file = NULL;

    /* initialize stuff */
    lv_rec_footer.frameCount = 0;
    main_header.fileCount = 0;

    /* open files */
    in_file = fopen(input_filename, "r");
    if(!in_file)
    {
        fprintf(stderr, "Failed to open file '%s'\n", input_filename);
        return 0;
    }

    if(output_filename)
    {
        frame_buffer = malloc(frame_buffer_size);
        if(!frame_buffer)
        {
            fprintf(stderr, "Failed to alloc mem\n");
            return 0;
        }
        
        out_file = fopen(output_filename, "w+");
        if(!out_file)
        {
            fprintf(stderr, "Failed to open file '%s'\n", output_filename);
            return 0;
        }
    }

    do
    {
        mlv_hdr_t buf;

read_headers:
        if(fread(&buf, sizeof(mlv_hdr_t), 1, in_file) != 1)
        {
            printf("[i] Reached end of file after %i blocks\n", blocks_processed);
            fclose(in_file);
            in_file = NULL;
            blocks_processed = 0;

            /* check for the next file M00, M01 etc */
            char seq_name[3];

            sprintf(seq_name, "%02d", seq_number);
            seq_number++;

            strcpy(&input_filename[strlen(input_filename) - 2], seq_name);

            /* try to open */
            in_file = fopen(input_filename, "r");
            if(!in_file)
            {
                break;
            }

            /* fine, it is available. so lets restart reading */
            printf("[i] Opened file '%s'\n", input_filename);

            goto read_headers;
        }

        fseeko(in_file, - (int64_t)sizeof(mlv_hdr_t), SEEK_CUR);

        /* file header */
        if(!memcmp(buf.blockType, "MLVI", 4))
        {
            mlv_file_hdr_t file_hdr;
            uint32_t hdr_size = MIN(sizeof(mlv_file_hdr_t), buf.blockSize);

            /* read the whole header block, but limit size to either our local type size or the written block size */
            if(fread(&file_hdr, hdr_size, 1, in_file) != 1)
            {
                fprintf(stderr, "[E] File ends in the middle of a block\n");
                return 0;
            }
            fseeko(in_file, file_hdr.blockSize - hdr_size, SEEK_CUR);

            if(verbose)
            {
                printf("File Header (MLVI)\n");
                printf("    Size        : 0x%08X\n", file_hdr.blockSize);
                printf("    Ver         : %s\n", file_hdr.versionString);
                printf("    GUID        : %08llX\n", file_hdr.fileGuid);
                printf("    FPS         : %f\n", (double)file_hdr.sourceFpsNom / (double)file_hdr.sourceFpsDenom);
                printf("    Frames Video: %d\n", file_hdr.videoFrameCount);
                printf("    Frames Audio: %d\n", file_hdr.audioFrameCount);
            }

            /* is this the first file? */
            if(main_header.fileCount == 0)
            {
                memcpy(&main_header, &file_hdr, sizeof(mlv_file_hdr_t));
            }
            else
            {
                /* no, its another chunk */
                if(main_header.fileGuid != file_hdr.fileGuid)
                {
                    printf("Error: GUID within the file chunks mismatch!\n");
                    break;
                }
            }

            if(raw_output)
            {
                lv_rec_footer.frameCount += file_hdr.videoFrameCount;
                lv_rec_footer.sourceFpsx1000 = (double)file_hdr.sourceFpsNom / (double)file_hdr.sourceFpsDenom * 1000;
                lv_rec_footer.frameSkip = 0;
            }
            
            if(mlv_output)
            {
                /* correct header size if needed */
                file_hdr.blockSize = sizeof(mlv_file_hdr_t);
                if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                {
                    fprintf(stderr, "[E] Failed writing into output file\n");
                    return 0;
                }
            }
        }
        else
        {
            uint64_t position = ftello(in_file);

            if(verbose)
            {
                printf("Block: %c%c%c%c\n", buf.blockType[0], buf.blockType[1], buf.blockType[2], buf.blockType[3]);
                printf("    Size: 0x%08X\n", buf.blockSize);
                printf("  Offset: 0x%llX\n", position);

                /* NULL blocks don't have timestamps */
                if(memcmp(buf.blockType, "NULL", 4))
                {
                    printf("    Time: %f ms\n", (double)buf.timestamp / 1000.0f);
                }
            }

            if(!memcmp(buf.blockType, "VIDF", 4))
            {
                mlv_vidf_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_vidf_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }

                if(verbose)
                {
                    printf("   Frame: #%d\n", block_hdr.frameNumber);
                    printf("    Crop: %dx%d\n", block_hdr.cropPosX, block_hdr.cropPosY);
                    printf("     Pan: %dx%d\n", block_hdr.panPosX, block_hdr.panPosY);
                    printf("   Space: %d\n", block_hdr.frameSpace);
                }
                
                if(raw_output)
                {
                    int frame_size = block_hdr.blockSize - sizeof(mlv_vidf_hdr_t) - block_hdr.frameSpace;
                    
                    fseeko(in_file, block_hdr.frameSpace, SEEK_CUR);
                    if(fread(frame_buffer, frame_size, 1, in_file) != 1)
                    {
                        fprintf(stderr, "[E] File ends in the middle of a block\n");
                        return 0;
                    }
                    lv_rec_footer.frameSize = frame_size;

                    fseeko(out_file, (uint64_t)block_hdr.frameNumber * (uint64_t)frame_size, SEEK_SET);
                    fwrite(frame_buffer, frame_size, 1, out_file);
                }
                else if(mlv_output)
                {
                    int frame_size = block_hdr.blockSize - sizeof(mlv_vidf_hdr_t) - block_hdr.frameSpace;
                    
                    fseeko(in_file, block_hdr.frameSpace, SEEK_CUR);
                    if(fread(frame_buffer, frame_size, 1, in_file) != 1)
                    {
                        fprintf(stderr, "[E] File ends in the middle of a block\n");
                        return 0;
                    }
                    
                    /* delete free space and correct header size if needed */
                    block_hdr.blockSize = sizeof(mlv_vidf_hdr_t) + frame_size;
                    block_hdr.frameSpace = 0;
                    
                    if(fwrite(&block_hdr, sizeof(mlv_vidf_hdr_t), 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                    if(fwrite(frame_buffer, frame_size, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
                else
                {
                    fseeko(in_file, block_hdr.blockSize - sizeof(mlv_vidf_hdr_t), SEEK_CUR);
                }

            }
            else if(!memcmp(buf.blockType, "LENS", 4))
            {
                mlv_lens_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_lens_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }
                
                /* skip remaining data, if there is any */
                fseeko(in_file, block_hdr.blockSize - hdr_size, SEEK_CUR);

                if(verbose)
                {
                    printf("     Name:        '%s'\n", block_hdr.lensName);
                    printf("     Serial:      '%s'\n", block_hdr.lensSerial);
                    printf("     Focal Len:   %d mm\n", block_hdr.focalLength);
                    printf("     Focus Dist:  %d mm\n", block_hdr.focalDist);
                    printf("     Aperture:    f/%.2f\n", (double)block_hdr.aperture / 100.0f);
                    printf("     IS Mode:     %d\n", block_hdr.stabilizerMode);
                    printf("     AF Mode:     %d\n", block_hdr.autofocusMode);
                    printf("     Lens ID:     0x%08X\n", block_hdr.lensID);
                    printf("     Flags:       0x%08X\n", block_hdr.flags);
                }
            
                if(mlv_output)
                {
                    /* correct header size if needed */
                    file_hdr.blockSize = sizeof(mlv_lens_hdr_t);
                    if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
            }
            else if(!memcmp(buf.blockType, "INFO", 4))
            {
                mlv_info_hdr_t block_hdr;
                int32_t hdr_size = MIN(sizeof(mlv_info_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }

                /* get the string length and malloc a buffer for that string */
                int str_length = block_hdr.blockSize - hdr_size;

                if(str_length)
                {
                    char *buf = malloc(str_length + 1);

                    if(fread(buf, str_length, 1, in_file) != 1)
                    {
                        fprintf(stderr, "[E] File ends in the middle of a block\n");
                        return 0;
                    }

                    if(verbose)
                    {
                        buf[str_length] = '\000';
                        printf("     String:   '%s'\n", buf);
                    }
                    
                    /* only output this block if there is any data */
                    if(mlv_output)
                    {
                        /* correct header size if needed */
                        file_hdr.blockSize = sizeof(mlv_info_hdr_t) + str_length;
                        if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                        {
                            fprintf(stderr, "[E] Failed writing into output file\n");
                            return 0;
                        }
                        if(fwrite(buf, str_length, 1, out_file) != 1)
                        {
                            fprintf(stderr, "[E] Failed writing into output file\n");
                            return 0;
                        }
                    }

                    free(buf);
                }
            
            }
            else if(!memcmp(buf.blockType, "ELVL", 4))
            {
                mlv_elvl_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_elvl_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }
                
                /* skip remaining data, if there is any */
                fseeko(in_file, block_hdr.blockSize - hdr_size, SEEK_CUR);

                if(verbose)
                {
                    printf("     Roll:    %2.2f\n", (double)block_hdr.roll / 100.0f);
                    printf("     Pitch:   %2.2f\n", (double)block_hdr.pitch / 100.0f);
                }
            
                if(mlv_output)
                {
                    /* correct header size if needed */
                    file_hdr.blockSize = sizeof(mlv_elvl_hdr_t);
                    if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
            }
            else if(!memcmp(buf.blockType, "WBAL", 4))
            {
                mlv_wbal_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_wbal_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }
                
                /* skip remaining data, if there is any */
                fseeko(in_file, block_hdr.blockSize - hdr_size, SEEK_CUR);

                if(verbose)
                {
                    printf("     Mode:   %d\n", block_hdr.wb_mode);
                    printf("     Kelvin:   %d\n", block_hdr.kelvin);
                    printf("     Gain R:   %d\n", block_hdr.wbgain_r);
                    printf("     Gain G:   %d\n", block_hdr.wbgain_g);
                    printf("     Gain B:   %d\n", block_hdr.wbgain_b);
                    printf("     Shift GM:   %d\n", block_hdr.wbs_gm);
                    printf("     Shift BA:   %d\n", block_hdr.wbs_ba);
                }
            
                if(mlv_output)
                {
                    /* correct header size if needed */
                    file_hdr.blockSize = sizeof(mlv_wbal_hdr_t);
                    if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
            }
            else if(!memcmp(buf.blockType, "IDNT", 4))
            {
                mlv_idnt_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_idnt_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }
                
                /* skip remaining data, if there is any */
                fseeko(in_file, block_hdr.blockSize - hdr_size, SEEK_CUR);

                if(verbose)
                {
                    printf("     Camera Name:   '%s'\n", block_hdr.cameraName);
                    printf("     Camera Serial: '%s'\n", block_hdr.cameraSerial);
                    printf("     Camera Model:  0x%08X\n", block_hdr.cameraModel);
                }
            
                if(mlv_output)
                {
                    /* correct header size if needed */
                    file_hdr.blockSize = sizeof(mlv_idnt_hdr_t);
                    if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
            }
            else if(!memcmp(buf.blockType, "RTCI", 4))
            {
                mlv_rtci_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_rtci_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }
                
                /* skip remaining data, if there is any */
                fseeko(in_file, block_hdr.blockSize - hdr_size, SEEK_CUR);

                if(verbose)
                {
                    printf("     Date:        %02d.%02d.%04d\n", block_hdr.tm_mday, block_hdr.tm_mon, 1900 + block_hdr.tm_year);
                    printf("     Time:        %02d:%02d:%02d (GMT+%d)\n", block_hdr.tm_hour, block_hdr.tm_min, block_hdr.tm_sec, block_hdr.tm_gmtoff);
                    printf("     Zone:        '%s'\n", block_hdr.tm_zone);
                    printf("     Day of week: %d\n", block_hdr.tm_wday);
                    printf("     Day of year: %d\n", block_hdr.tm_yday);
                    printf("     Daylight s.: %d\n", block_hdr.tm_isdst);
                }
            
                if(mlv_output)
                {
                    /* correct header size if needed */
                    file_hdr.blockSize = sizeof(mlv_rtci_hdr_t);
                    if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
            }
            else if(!memcmp(buf.blockType, "EXPO", 4))
            {
                mlv_expo_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_expo_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }
                
                /* skip remaining data, if there is any */
                fseeko(in_file, block_hdr.blockSize - hdr_size, SEEK_CUR);

                if(verbose)
                {
                    printf("     ISO Mode:   %d\n", block_hdr.isoMode);
                    printf("     ISO:        %d\n", block_hdr.isoValue);
                    printf("     ISO Analog: %d\n", block_hdr.isoAnalog);
                    printf("     ISO DGain:  %d EV\n", block_hdr.digitalGain);
                    printf("     Shutter:    %llu µs (1/%.2f)\n", block_hdr.shutterValue, 1000000.0f/block_hdr.shutterValue);
                }
            
                if(mlv_output)
                {
                    /* correct header size if needed */
                    file_hdr.blockSize = sizeof(mlv_expo_hdr_t);
                    if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
            }
            else if(!memcmp(buf.blockType, "RAWI", 4))
            {
                mlv_rawi_hdr_t block_hdr;
                uint32_t hdr_size = MIN(sizeof(mlv_rawi_hdr_t), buf.blockSize);

                if(fread(&block_hdr, hdr_size, 1, in_file) != 1)
                {
                    fprintf(stderr, "[E] File ends in the middle of a block\n");
                    return 0;
                }
                
                /* skip remaining data, if there is any */
                fseeko(in_file, block_hdr.blockSize - hdr_size, SEEK_CUR);

                if(verbose)
                {
                    printf("    Res:  %dx%d\n", block_hdr.xRes, block_hdr.yRes);
                    printf("    raw_info:\n");
                    printf("      api_version      0x%08X\n", block_hdr.raw_info.api_version);
                    printf("      height           %d\n", block_hdr.raw_info.height);
                    printf("      width            %d\n", block_hdr.raw_info.width);
                    printf("      pitch            %d\n", block_hdr.raw_info.pitch);
                    printf("      frame_size       0x%08X\n", block_hdr.raw_info.frame_size);
                    printf("      bits_per_pixel   %d\n", block_hdr.raw_info.bits_per_pixel);
                    printf("      black_level      %d\n", block_hdr.raw_info.black_level);
                    printf("      white_level      %d\n", block_hdr.raw_info.white_level);
                    printf("      active_area.y1   %d\n", block_hdr.raw_info.active_area.y1);
                    printf("      active_area.x1   %d\n", block_hdr.raw_info.active_area.x1);
                    printf("      active_area.y2   %d\n", block_hdr.raw_info.active_area.y2);
                    printf("      active_area.x2   %d\n", block_hdr.raw_info.active_area.x2);
                    printf("      exposure_bias    %d, %d\n", block_hdr.raw_info.exposure_bias[0], block_hdr.raw_info.exposure_bias[1]);
                    printf("      cfa_pattern      0x%08X\n", block_hdr.raw_info.cfa_pattern);
                    printf("      calibration_ill  %d\n", block_hdr.raw_info.calibration_illuminant1);
                }

                if(raw_output)
                {
                    strncpy((char*)lv_rec_footer.magic, "RAWM", 4);
                    lv_rec_footer.xRes = block_hdr.xRes;
                    lv_rec_footer.yRes = block_hdr.yRes;
                    lv_rec_footer.raw_info = block_hdr.raw_info;
                }
            
                if(mlv_output)
                {
                    /* correct header size if needed */
                    file_hdr.blockSize = sizeof(mlv_rawi_hdr_t);
                    if(fwrite(&file_hdr, file_hdr.blockSize, 1, out_file) != 1)
                    {
                        fprintf(stderr, "[E] Failed writing into output file\n");
                        return 0;
                    }
                }
            }
            else
            {
                fseeko(in_file, buf.blockSize, SEEK_CUR);
            }
        }
        
        /* count any read block, no matter if header or video frame */
        blocks_processed++;
    }
    while(!feof(in_file));

    if(in_file)
    {
        fclose(in_file);
    }

    if(raw_output)
    {
        fseeko(out_file, 0, SEEK_END);
        fwrite(&lv_rec_footer, sizeof(lv_rec_file_footer_t), 1, out_file);
        fclose(out_file);
    }

    if(output_filename)
    {
        free(output_filename);
    }
    return 0;
}