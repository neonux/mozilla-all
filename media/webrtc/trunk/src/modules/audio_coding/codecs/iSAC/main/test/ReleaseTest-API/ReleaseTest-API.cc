/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ReleaseTest-API.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <iostream>

/* include API */
#include "isac.h"
#include "utility.h"

/* Defines */
#define SEED_FILE "randseed.txt"  /* Used when running decoder on garbage data  */
#define MAX_FRAMESAMPLES     960  /* max number of samples per frame
                                      (= 60 ms frame & 16 kHz) or
                                      (= 30 ms frame & 32 kHz)                  */
#define FRAMESAMPLES_10ms	 160   /* number of samples per 10ms frame          */
#define SWBFRAMESAMPLES_10ms 320
//#define FS		        	16000 /* sampling frequency (Hz) */

#ifdef WIN32
#define CLOCKS_PER_SEC      1000  /* Runtime statistics */
#endif




using namespace std;

int main(int argc, char* argv[])
{

    char inname[100], outname[100], bottleneck_file[100], vadfile[100];
	FILE *inp, *outp, *f_bn=NULL, *vadp, *bandwidthp;
	int framecnt, endfile;

	int i, errtype, VADusage = 0, packetLossPercent = 0;
	WebRtc_Word16 CodingMode;
	WebRtc_Word32 bottleneck;
	WebRtc_Word16 framesize = 30;           /* ms */
	int cur_framesmpls, err;

	/* Runtime statistics */
	double starttime, runtime, length_file;

	WebRtc_Word16 stream_len = 0;
	WebRtc_Word16 declen, lostFrame = 0, declenTC = 0;

	WebRtc_Word16 shortdata[SWBFRAMESAMPLES_10ms];
	WebRtc_Word16 vaddata[SWBFRAMESAMPLES_10ms*3];
	WebRtc_Word16 decoded[MAX_FRAMESAMPLES << 1];
	WebRtc_Word16 decodedTC[MAX_FRAMESAMPLES << 1];
	WebRtc_UWord16 streamdata[500];
	WebRtc_Word16	speechType[1];
    WebRtc_Word16 rateBPS = 0;
    WebRtc_Word16 fixedFL = 0;
    WebRtc_Word16 payloadSize = 0;
    WebRtc_Word32 payloadRate = 0;
    int setControlBWE = 0;
    short FL, testNum;
	char version_number[20];
    FILE  *plFile;
    WebRtc_Word32 sendBN;

#ifdef _DEBUG
	FILE *fy;
	double kbps;
#endif /* _DEBUG */
	int totalbits =0;
	int totalsmpls =0;

    /* If use GNS file */
    FILE *fp_gns = NULL;
	char gns_file[100];
    short maxStreamLen30 = 0;
    short maxStreamLen60 = 0;
    short sampFreqKHz = 32;
    short samplesIn10Ms;
    short useAssign = 0;
    //FILE logFile;
    bool doTransCoding = false;
    WebRtc_Word32 rateTransCoding = 0;
    WebRtc_UWord16 streamDataTransCoding[600];
    WebRtc_Word16 streamLenTransCoding = 0;
    FILE* transCodingFile = NULL;
    FILE* transcodingBitstream = NULL;
    WebRtc_UWord32 numTransCodingBytes = 0;

	/* only one structure used for ISAC encoder */
	ISACStruct* ISAC_main_inst;
    ISACStruct* decoderTransCoding;

	BottleNeckModel       BN_data;

#ifdef _DEBUG
	fy = fopen("bit_rate.dat", "w");
	fclose(fy);
	fy = fopen("bytes_frames.dat", "w");
	fclose(fy);
#endif /* _DEBUG */

	/* Handling wrong input arguments in the command line */
	if((argc<3) || (argc>17))  {
		printf("\n\nWrong number of arguments or flag values.\n\n");

        printf("\n");
        WebRtcIsac_version(version_number);
        printf("iSAC-swb version %s \n\n", version_number);

        printf("Usage:\n\n");
        printf("./kenny.exe [-I] bottleneck_value infile outfile \n\n");
        printf("with:\n");
        printf("[-FS num]       :   sampling frequency in kHz, valid values are 16 & 32,\n");
        printf("                    with 16 as default.\n");
        printf("[-I]            :   if -I option is specified, the coder will use\n");
        printf("                    an instantaneous Bottleneck value. If not, it\n");
        printf("                    will be an adaptive Bottleneck value.\n\n");
        printf("[-assign]       :   Use Assign API.\n");
        printf("[-B num]        :   the value of the bottleneck provided either\n");
        printf("                    as a fixed value in bits/sec (e.g. 25000) or\n");
        printf("                    read from a file (e.g. bottleneck.txt)\n\n");
        printf("[-INITRATE num] :   Set a new value for initial rate. Note! Only used in \n");
        printf("                    adaptive mode.\n\n");
        printf("[-FL num]       :   Set (initial) frame length in msec. Valid length are \n");
        printf("                    30 and 60 msec.\n\n");
        printf("[-FIXED_FL]     :   Frame length will be fixed to initial value.\n\n");
        printf("[-MAX num]      :   Set the limit for the payload size of iSAC in bytes. \n");
        printf("                    Minimum 100 maximum 400.\n\n");
        printf("[-MAXRATE num]  :   Set the maxrate for iSAC in bits per second. \n");
        printf("                    Minimum 32000, maximum 53400.\n\n");
        printf("[-F num]        :   if -F option is specified, the test function\n");
        printf("                    will run the iSAC API fault scenario specified by the\n");
        printf("                    supplied number.\n");
        printf("                    F 1 - Call encoder prior to init encoder call\n");
        printf("                    F 2 - Call decoder prior to init decoder call\n");
        printf("                    F 3 - Call decoder prior to encoder call\n");
        printf("                    F 4 - Call decoder with a too short coded sequence\n");
        printf("                    F 5 - Call decoder with a too long coded sequence\n");
        printf("                    F 6 - Call decoder with random bit stream\n");
        printf("                    F 7 - Call init encoder/decoder at random during a call\n");
        printf("                    F 8 - Call encoder/decoder without having allocated memory \n");
        printf("                          for encoder/decoder instance\n");
        printf("                    F 9 - Call decodeB without calling decodeA\n");
        printf("                    F 10 - Call decodeB with garbage data\n");
        printf("[-PL num]       :   if -PL option is specified \n");
        printf("[-T rate file]  :   test trans-coding with target bottleneck 'rate' bits/sec\n");
        printf("                    the output file is written to 'file'\n");
        printf("[-LOOP num]     :   number of times to repeat coding the input file for stress testing\n");
        //printf("[-CE num]       :   Test of APIs used by Conference Engine.\n");
        //printf("                    CE 1 - getNewBitstream, getBWE \n");
        //printf("                    (CE 2 - RESERVED for transcoding)\n");
        //printf("                    CE 3 - getSendBWE, setSendBWE.  \n\n");
        //printf("-L filename     :   write the logging info into file (appending)\n");
        printf("infile          :   Normal speech input file\n\n");
        printf("outfile         :   Speech output file\n\n");
    	exit(0);
	}

    /* Print version number */
    printf("-------------------------------------------------\n");
    WebRtcIsac_version(version_number);
    printf("iSAC version %s \n\n", version_number);

    /* Loop over all command line arguments */
	CodingMode = 0;
	testNum = 0;
    useAssign = 0;
    //logFile = NULL;
    char transCodingFileName[500];
    WebRtc_Word16 totFileLoop = 0;
    WebRtc_Word16 numFileLoop = 0;
	for (i = 1; i < argc-2;i++)
    {
        if(!strcmp("-LOOP", argv[i]))
        {
            i++;
            totFileLoop = (WebRtc_Word16)atol(argv[i]);
            if(totFileLoop <= 0)
            {
                fprintf(stderr, "Invalid number of runs for the given input file, %d.", totFileLoop);
                exit(0);
            }
        }

        if(!strcmp("-T", argv[i]))
        {
            doTransCoding = true;
            i++;
            rateTransCoding = atoi(argv[i]);
            i++;
            strcpy(transCodingFileName, argv[i]);
        }

        /*Should we use assign API*/
        if(!strcmp("-assign", argv[i]))
        {
            useAssign = 1;
        }

        /* Set Sampling Rate */
        if(!strcmp("-FS", argv[i]))
        {
            i++;
            sampFreqKHz = atoi(argv[i]);
        }

        /* Instantaneous mode */
		if(!strcmp ("-I", argv[i]))
        {
			printf("Instantaneous BottleNeck\n");
			CodingMode = 1;
		}

        /* Set (initial) bottleneck value */
        if(!strcmp ("-INITRATE", argv[i]))	{
			rateBPS = atoi(argv[i + 1]);
            setControlBWE = 1;
            if((rateBPS < 10000) || (rateBPS > 32000))
            {
				printf("\n%d is not a initial rate. Valid values are in the range 10000 to 32000.\n", rateBPS);
				exit(0);
            }
			printf("New initial rate: %d\n", rateBPS);
			i++;
		}

        /* Set (initial) framelength */
        if(!strcmp ("-FL", argv[i]))	{
			framesize = atoi(argv[i + 1]);
            if((framesize != 30) && (framesize != 60))
            {
				printf("\n%d is not a valid frame length. Valid length are 30 and 60 msec.\n", framesize);
				exit(0);
            }
            setControlBWE = 1;
			printf("Frame Length: %d\n", framesize);
			i++;
		}

        /* Fixed frame length */
        if(!strcmp ("-FIXED_FL", argv[i]))
        {
			fixedFL = 1;
            setControlBWE = 1;
			printf("Fixed Frame Length\n");
		}

        /* Set maximum allowed payload size in bytes */
        if(!strcmp ("-MAX", argv[i]))	{
			payloadSize = atoi(argv[i + 1]);
            printf("Maximum Payload Size: %d\n", payloadSize);
			i++;
		}

        /* Set maximum rate in bytes */
        if(!strcmp ("-MAXRATE", argv[i]))	{
			payloadRate = atoi(argv[i + 1]);
            printf("Maximum Rate in kbps: %d\n", payloadRate);
			i++;
		}

        /* Test of fault scenarious */
        if(!strcmp ("-F", argv[i]))
        {
			testNum = atoi(argv[i + 1]);
			printf("Fault test: %d\n", testNum);
			if(testNum < 1 || testNum > 10)
            {
				printf("\n%d is not a valid Fault Scenario number. Valid Fault Scenarios are numbered 1-10.\n", testNum);
				exit(0);
			}
			i++;
		}

        /* Packet loss test */
		if(!strcmp ("-PL", argv[i]))
        {
			if( isdigit( *argv[i+1] ) )
            {
				packetLossPercent = atoi( argv[i+1] );
				if( (packetLossPercent < 0) | (packetLossPercent > 100) )
                {
					printf( "\nInvalid packet loss perentage \n" );
					exit( 0 );
				}
                if( packetLossPercent > 0 )
                {
					printf( "Simulating %d %% of independent packet loss\n", packetLossPercent );
                }
                else
                {
					printf( "\nNo Packet Loss Is Simulated \n" );
                }
            }
            else
            {
				plFile = fopen( argv[i+1], "rb" );
				if( plFile == NULL )
                {
					printf( "\n couldn't open the frameloss file: %s\n", argv[i+1] );
					exit( 0 );
				}
				printf( "Simulating packet loss through the given channel file: %s\n", argv[i+1] );
			}
			i++;
		}

        /* Random packetlosses */
		if(!strcmp ("-rnd", argv[i]))
        {
			srand((unsigned int)time(NULL) );
			printf( "Random pattern in lossed packets \n" );
		}

        /* Use gns file */
		if(!strcmp ("-G", argv[i]))
        {
			sscanf(argv[i + 1], "%s", gns_file);
			fp_gns = fopen(gns_file, "rb");
			if(fp_gns  == NULL)
            {
				printf("Cannot read file %s.\n", gns_file);
				exit(0);
			}
			i++;
		}


        // make it with '-B'
        /* Get Bottleneck value */
        if(!strcmp("-B", argv[i]))
        {
            i++;
            bottleneck = atoi(argv[i]);
            if(bottleneck == 0)
            {
                sscanf(argv[i], "%s", bottleneck_file);
                f_bn = fopen(bottleneck_file, "rb");
                if(f_bn  == NULL)
                {
                    printf("Error No value provided for BottleNeck and cannot read file %s.\n", bottleneck_file);
                    exit(0);
                }
                else
                {
                    printf("reading bottleneck rates from file %s\n\n",bottleneck_file);
                    if(fscanf(f_bn, "%d", &bottleneck) == EOF)
                    {
                        /* Set pointer to beginning of file */
                        fseek(f_bn, 0L, SEEK_SET);
                        if (fscanf(f_bn, "%d", &bottleneck) == EOF) {
                            exit(0);
                        }
                    }

                    /*	Bottleneck is a cosine function
                    *	Matlab code for writing the bottleneck file:
                    *	BottleNeck_10ms = 20e3 + 10e3 * cos((0:5999)/5999*2*pi);
                    *	fid = fopen('bottleneck.txt', 'wb');
                    *	fprintf(fid, '%d\n', BottleNeck_10ms); fclose(fid);
                    */
                }
            }
            else
            {
                printf("\nfixed bottleneck rate of %d bits/s\n\n", bottleneck);
            }
        }
        /* Run Conference Engine APIs */
        //     Do not test it in the first release
        //
        //     if(!strcmp ("-CE", argv[i]))
        //     {
        //         testCE = atoi(argv[i + 1]);
        //         if(testCE==1)
        //         {
        //             i++;
        //             scale = (float)atof( argv[i+1] );
        //         }
        //         else if(testCE == 2)
        //         {
        //             printf("\nCE-test 2 (transcoding) not implemented.\n");
        //             exit(0);
        //         }
        //         else if(testCE < 1 || testCE > 3)
        //         {
        //             printf("\n%d is not a valid CE-test number. Valid CE tests are 1-3.\n", testCE);
        //             exit(0);
        //         }
        //         printf("CE-test number: %d\n", testCE);
        //         i++;
        //     }
    }

	if(CodingMode == 0)
	{
		printf("\nAdaptive BottleNeck\n");
	}

    switch(sampFreqKHz)
    {
    case 16:
        {
            printf("iSAC Wideband.\n");
            samplesIn10Ms = FRAMESAMPLES_10ms;
            break;
        }
    case 32:
        {
            printf("iSAC Supper-Wideband.\n");
            samplesIn10Ms = SWBFRAMESAMPLES_10ms;
            break;
        }
    default:
            printf("Unsupported sampling frequency %d kHz", sampFreqKHz);
            exit(0);
    }




	/* Get Input and Output files */
	sscanf(argv[argc-2], "%s", inname);
	sscanf(argv[argc-1], "%s", outname);
    printf("\nInput file: %s\n", inname);
    printf("Output file: %s\n\n", outname);
	if((inp = fopen(inname,"rb")) == NULL)
    {
		printf("  Error iSAC Cannot read file %s.\n", inname);
        cout << flush;
		exit(1);
	}

	if((outp = fopen(outname,"wb")) == NULL)
    {
		printf("  Error iSAC Cannot write file %s.\n", outname);
        cout << flush;
        getchar();
		exit(1);
	}
	if(VADusage)
    {
		if((vadp = fopen(vadfile,"rb")) == NULL)
        {
			printf("  Error iSAC Cannot read file %s.\n", vadfile);
            cout << flush;
			exit(1);
		}
	}

    if((bandwidthp = fopen("bwe.pcm","wb")) == NULL)
    {
            printf("  Error iSAC Cannot read file %s.\n", "bwe.pcm");
            cout << flush;
            exit(1);
    }


	starttime = clock()/(double)CLOCKS_PER_SEC; /* Runtime statistics */

    /* Initialize the ISAC and BN structs */
    if(testNum != 8)
    {
        if(!useAssign)
        {
            err =WebRtcIsac_Create(&ISAC_main_inst);
            WebRtcIsac_SetEncSampRate(ISAC_main_inst, (sampFreqKHz == 16)? kIsacWideband:kIsacSuperWideband);
            WebRtcIsac_SetDecSampRate(ISAC_main_inst, (sampFreqKHz == 16)? kIsacWideband:kIsacSuperWideband);
        }
        else
        {
            /* Test the Assign functions */
            int sss;
            void *ppp;
            err = WebRtcIsac_AssignSize(&sss);
            ppp = malloc(sss);
            err = WebRtcIsac_Assign(&ISAC_main_inst, ppp);
            WebRtcIsac_SetEncSampRate(ISAC_main_inst, (sampFreqKHz == 16)? kIsacWideband:kIsacSuperWideband);
            WebRtcIsac_SetDecSampRate(ISAC_main_inst, (sampFreqKHz == 16)? kIsacWideband:kIsacSuperWideband);
        }
        /* Error check */
        if(err < 0)
        {
            printf("\n\n Error in create.\n\n");
            cout << flush;
            exit(EXIT_FAILURE);
        }
    }
	BN_data.arrival_time  = 0;
	BN_data.sample_count  = 0;
	BN_data.rtp_number    = 0;

	/* Initialize encoder and decoder */
    framecnt= 0;
    endfile	= 0;

    if(doTransCoding)
    {
        WebRtcIsac_Create(&decoderTransCoding);
        WebRtcIsac_SetEncSampRate(decoderTransCoding, (sampFreqKHz == 16)? kIsacWideband:kIsacSuperWideband);
        WebRtcIsac_SetDecSampRate(decoderTransCoding, (sampFreqKHz == 16)? kIsacWideband:kIsacSuperWideband);
        WebRtcIsac_DecoderInit(decoderTransCoding);
        transCodingFile = fopen(transCodingFileName, "wb");
        if(transCodingFile == NULL)
        {
            printf("Could not open %s to output trans-coding.\n", transCodingFileName);
            exit(0);
        }
        strcat(transCodingFileName, ".bit");
        transcodingBitstream = fopen(transCodingFileName, "wb");
        if(transcodingBitstream == NULL)
        {
            printf("Could not open %s to write the bit-stream of transcoder.\n", transCodingFileName);
            exit(0);
        }
    }

    if(testNum != 1)
    {
		if(WebRtcIsac_EncoderInit(ISAC_main_inst, CodingMode) < 0)
        {
            printf("Error could not initialize the encoder \n");
            cout << flush;
            return 0;
        }
	}
    if(testNum != 2)
    {
        if(WebRtcIsac_DecoderInit(ISAC_main_inst) < 0)
        {
            printf("Error could not initialize the decoder \n");
            cout << flush;
            return 0;
        }
	}
	if(CodingMode == 1)
    {
        err = WebRtcIsac_Control(ISAC_main_inst, bottleneck, framesize);
        if(err < 0)
        {
            /* exit if returned with error */
            errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
            printf("\n\n Error in initialization (control): %d.\n\n", errtype);
            cout << flush;
            if(testNum == 0)
            {
                exit(EXIT_FAILURE);
            }
        }
	}

    if((setControlBWE) && (CodingMode == 0))
    {
        err = WebRtcIsac_ControlBwe(ISAC_main_inst, rateBPS, framesize, fixedFL);
        if(err < 0)
        {
            /* exit if returned with error */
            errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);

            printf("\n\n Error in Control BWE: %d.\n\n", errtype);
            cout << flush;
            exit(EXIT_FAILURE);
        }
    }

    if(payloadSize != 0)
    {
        err = WebRtcIsac_SetMaxPayloadSize(ISAC_main_inst, payloadSize);
        if(err < 0)
        {
            /* exit if returned with error */
            errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
            printf("\n\n Error in SetMaxPayloadSize: %d.\n\n", errtype);
            cout << flush;
            exit(EXIT_FAILURE);
        }
    }
    if(payloadRate != 0)
    {
        err = WebRtcIsac_SetMaxRate(ISAC_main_inst, payloadRate);
        if(err < 0)
        {
            /* exit if returned with error */
            errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
            printf("\n\n Error in SetMaxRateInBytes: %d.\n\n", errtype);
            cout << flush;
            exit(EXIT_FAILURE);
        }
    }

	*speechType = 1;

    cout << "\n" << flush;

    length_file = 0;
    WebRtc_Word16 bnIdxTC;
    WebRtc_Word16 jitterInfoTC;
    while (endfile == 0)
    {
        /* Call init functions at random, fault test number 7 */
		if(testNum == 7 && (rand()%2 == 0))
        {
            err = WebRtcIsac_EncoderInit(ISAC_main_inst, CodingMode);
            /* Error check */
            if(err < 0)
            {
                errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
                printf("\n\n Error in encoderinit: %d.\n\n", errtype);
                cout << flush;
            }

            err = WebRtcIsac_DecoderInit(ISAC_main_inst);
            /* Error check */
            if(err < 0)
            {
                errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
                printf("\n\n Error in decoderinit: %d.\n\n", errtype);
                cout << flush;
            }
        }

		cur_framesmpls = 0;
		while (1)
        {
            /* Read 10 ms speech block */
            endfile = readframe(shortdata, inp, samplesIn10Ms);

            if(endfile)
            {
                numFileLoop++;
                if(numFileLoop < totFileLoop)
                {
                    rewind(inp);
                    framecnt = 0;
                    fprintf(stderr, "\n");
                    endfile = readframe(shortdata, inp, samplesIn10Ms);
                }
            }

            if(testNum == 7)
            {
		    	srand((unsigned int)time(NULL));
		    }

            /* iSAC encoding */
            if(!(testNum == 3 && framecnt == 0))
            {
                stream_len = WebRtcIsac_Encode(ISAC_main_inst,
                    shortdata,
                    (WebRtc_Word16*)streamdata);
                if((payloadSize != 0) && (stream_len > payloadSize))
                {
                    if(testNum == 0)
                    {
                        printf("\n\n");
                    }

                    printf("\nError: Streamsize out of range %d\n", stream_len - payloadSize);
                    cout << flush;
                }

                WebRtcIsac_GetUplinkBw(ISAC_main_inst, &sendBN);

                if(stream_len>0)
                {
                    if(doTransCoding)
                    {
                        WebRtc_Word16 indexStream;
                        WebRtc_UWord8 auxUW8;

                        /************************* Main Transcoding stream *******************************/
                        WebRtcIsac_GetDownLinkBwIndex(ISAC_main_inst, &bnIdxTC, &jitterInfoTC);
                        streamLenTransCoding = WebRtcIsac_GetNewBitStream(
                            ISAC_main_inst, bnIdxTC, jitterInfoTC, rateTransCoding,
                            (WebRtc_Word16*)streamDataTransCoding, false);
                        if(streamLenTransCoding < 0)
                        {
                            fprintf(stderr, "Error in trans-coding\n");
                            exit(0);
                        }
                        auxUW8 = (WebRtc_UWord8)(((streamLenTransCoding & 0xFF00) >> 8) &  0x00FF);
                        fwrite(&auxUW8, sizeof(WebRtc_UWord8),
                            1, transcodingBitstream);

                        auxUW8 = (WebRtc_UWord8)(streamLenTransCoding & 0x00FF);
                        fwrite(&auxUW8, sizeof(WebRtc_UWord8),
                            1, transcodingBitstream);

                        fwrite((WebRtc_UWord8*)streamDataTransCoding, sizeof(WebRtc_UWord8),
                            streamLenTransCoding, transcodingBitstream);

                        WebRtcIsac_ReadBwIndex((WebRtc_Word16*)streamDataTransCoding, &indexStream);
                        if(indexStream != bnIdxTC)
                        {
                            fprintf(stderr, "Error in inserting Bandwidth index into transcoding stream.\n");
                            exit(0);
                        }
                        numTransCodingBytes += streamLenTransCoding;
                    }
                }
            }
            else
            {
                break;
            }

			if(stream_len < 0)
            {
				/* exit if returned with error */
				errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
                printf("\n\nError in encoder: %d.\n\n", errtype);
                cout << flush;
			}
			cur_framesmpls += samplesIn10Ms;
			/* exit encoder loop if the encoder returned a bitstream */
			if(stream_len != 0) break;
		}

        /* read next bottleneck rate */
        if(f_bn != NULL)
        {
            if(fscanf(f_bn, "%d", &bottleneck) == EOF)
            {
                /* Set pointer to beginning of file */
                fseek(f_bn, 0L, SEEK_SET);
                if (fscanf(f_bn, "%d", &bottleneck) == EOF) {
                    exit(0);
                }
            }
            if(CodingMode == 1)
            {
                WebRtcIsac_Control(ISAC_main_inst, bottleneck, framesize);
            }
        }

        length_file += cur_framesmpls;
        if(cur_framesmpls == (3 * samplesIn10Ms))
        {
            maxStreamLen30 = (stream_len > maxStreamLen30)? stream_len:maxStreamLen30;
        }
        else
        {
            maxStreamLen60 = (stream_len > maxStreamLen60)? stream_len:maxStreamLen60;
        }

        if(!lostFrame)
        {
            lostFrame = ((rand()%100) < packetLossPercent);
        }
        else
        {
            lostFrame = 0;
        }

        // RED.
        if(lostFrame)
        {
            stream_len = WebRtcIsac_GetRedPayload(ISAC_main_inst,
                (WebRtc_Word16*)streamdata);

            if(doTransCoding)
            {
                streamLenTransCoding = WebRtcIsac_GetNewBitStream(
                    ISAC_main_inst, bnIdxTC, jitterInfoTC, rateTransCoding,
                    (WebRtc_Word16*)streamDataTransCoding, true);
                if(streamLenTransCoding < 0)
                {
                    fprintf(stderr, "Error in RED trans-coding\n");
                    exit(0);
                }
            }
        }

        /* make coded sequence to short be inreasing */
		/* the length the decoder expects */
		if(testNum == 4)
        {
			stream_len += 10;
		}

		/* make coded sequence to long be decreasing */
		/* the length the decoder expects */
		if(testNum == 5)
        {
			stream_len -= 10;
		}

        if(testNum == 6)
        {
			srand((unsigned int)time(NULL));
            for(i = 0; i < stream_len; i++)
            {
				streamdata[i] = rand();
            }
		}

        if(VADusage){
            readframe(vaddata, vadp, samplesIn10Ms*3);
        }

		/* simulate packet handling through NetEq and the modem */
		if(!(testNum == 3 && framecnt == 0))
        {
            get_arrival_time(cur_framesmpls, stream_len, bottleneck, &BN_data,
                sampFreqKHz*1000, sampFreqKHz*1000);
        }

		if(VADusage && (framecnt>10 && vaddata[0]==0))
        {
			BN_data.rtp_number--;
		}
        else
        {
            /* Error test number 10, garbage data */
            if(testNum == 10)
            {
                /* Test to run decoder with garbage data */
                for(i = 0; i < stream_len; i++)
                {
                    streamdata[i] = (short) (streamdata[i]) + (short) rand();
                }
            }

            if(testNum != 9)
            {
                err = WebRtcIsac_UpdateBwEstimate(ISAC_main_inst, streamdata,
                    stream_len, BN_data.rtp_number, BN_data.sample_count,
                    BN_data.arrival_time);

                if(err < 0)
                {
                    /* exit if returned with error */
                    errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
                    if(testNum == 0)
                    {
                        printf("\n\n");
                    }

                    printf("Error: in decoder: %d.", errtype);
                    cout << flush;
                    if(testNum == 0)
                    {
                        printf("\n\n");
                    }

                }
            }

            /* Call getFramelen, only used here for function test */
            err = WebRtcIsac_ReadFrameLen(ISAC_main_inst,
                (WebRtc_Word16*)streamdata, &FL);
            if(err < 0)
            {
                /* exit if returned with error */
                errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
                if(testNum == 0)
                {
                    printf("\n\n");
                }
                printf("    Error: in getFrameLen %d.", errtype);
                cout << flush;
                if(testNum == 0)
                {
                    printf("\n\n");
                }
            }

            // iSAC decoding

            if(lostFrame)
            {
                declen = WebRtcIsac_DecodeRcu(ISAC_main_inst, streamdata,
                    stream_len, decoded, speechType);

                if(doTransCoding)
                {
                    declenTC = WebRtcIsac_DecodeRcu(decoderTransCoding,
                        streamDataTransCoding, streamLenTransCoding,
                        decodedTC, speechType);
                }
            }
            else
            {
                declen = WebRtcIsac_Decode(ISAC_main_inst, streamdata,
                    stream_len, decoded, speechType);

                if(doTransCoding)
                {
                    declenTC = WebRtcIsac_Decode(decoderTransCoding,
                        streamDataTransCoding, streamLenTransCoding,
                        decodedTC, speechType);
                }
            }

            if(declen < 0)
            {
                /* exit if returned with error */
                errtype=WebRtcIsac_GetErrorCode(ISAC_main_inst);
                if(testNum == 0)
                {
                    printf("\n\n");
                }
                printf("    Error: in decoder %d.", errtype);
                cout << flush;
                if(testNum == 0)
                {
                    printf("\n\n");
                }
            }

            if(declenTC < 0)
            {
                if(testNum == 0)
                {
                    printf("\n\n");
                }
                printf("    Error: in decoding the transcoded stream");
                cout << flush;
                if(testNum == 0)
                {
                    printf("\n\n");
                }

            }
        }
        /* Write decoded speech frame to file */
        if((declen > 0) && (numFileLoop == 0))
        {
            fwrite(decoded, sizeof(WebRtc_Word16), declen, outp);
        }

        if((declenTC > 0) && (numFileLoop == 0))
        {
            fwrite(decodedTC, sizeof(WebRtc_Word16), declen, transCodingFile);
        }


		fprintf(stderr, "\rframe = %5d  ", framecnt);
        fflush(stderr);
		framecnt++;

        /* Error test number 10, garbage data */
        //if(testNum == 10)
        //{
        //    /* Test to run decoder with garbage data */
        //    if( (seedfile = fopen(SEED_FILE, "a+t") ) == NULL )
        //    {
        //        fprintf(stderr, "Error: Could not open file %s\n", SEED_FILE);
        //    }
        //    else
        //    {
        //        fprintf(seedfile, "ok\n\n");
        //        fclose(seedfile);
        //    }
        //}
        /* Error test number 10, garbage data */
        //if(testNum == 10)
        //{
        //    /* Test to run decoder with garbage data */
        //    for ( i = 0; i < stream_len; i++)
        //    {
        //        streamdata[i] = (short) (streamdata[i] + (short) rand());
        //    }
        //}


		totalsmpls += declen;
		totalbits += 8 * stream_len;
#ifdef _DEBUG
        kbps = ((double) sampFreqKHz * 1000.) / ((double) cur_framesmpls) * 8.0 * stream_len / 1000.0;// kbits/s
		fy = fopen("bit_rate.dat", "a");
		fprintf(fy, "Frame %i = %0.14f\n", framecnt, kbps);
		fclose(fy);

#endif /* _DEBUG */

	}
	printf("\n");
	printf("total bits               = %d bits\n", totalbits);
	printf("measured average bitrate = %0.3f kbits/s\n",
        (double)totalbits *(sampFreqKHz) / totalsmpls);
    if(doTransCoding)
    {
        printf("Transcoding average bit-rate = %0.3f kbps\n",
            (double)numTransCodingBytes * 8.0 *(sampFreqKHz) / totalsmpls);
        fclose(transCodingFile);
    }
	printf("\n");

	/* Runtime statistics */
	runtime = (double)(clock()/(double)CLOCKS_PER_SEC-starttime);
	length_file = length_file /(sampFreqKHz * 1000.);

    printf("\n\nLength of speech file: %.1f s\n", length_file);
	printf("Time to run iSAC:      %.2f s (%.2f %% of realtime)\n\n", runtime, (100*runtime/length_file));

    if(maxStreamLen30 != 0)
    {
        printf("Maximum payload size 30ms Frames %d bytes (%0.3f kbps)\n",
            maxStreamLen30,
            maxStreamLen30 * 8 / 30.);
    }
    if(maxStreamLen60 != 0)
    {
        printf("Maximum payload size 60ms Frames %d bytes (%0.3f kbps)\n",
            maxStreamLen60,
            maxStreamLen60 * 8 / 60.);
    }
    //fprintf(stderr, "\n");

	fprintf(stderr, "   %.1f s", length_file);
    fprintf(stderr, "   %0.1f kbps", (double)totalbits *(sampFreqKHz) / totalsmpls);
    if(maxStreamLen30 != 0)
    {
        fprintf(stderr, "   plmax-30ms %d bytes (%0.0f kbps)",
            maxStreamLen30,
            maxStreamLen30 * 8 / 30.);
    }
    if(maxStreamLen60 != 0)
    {
        fprintf(stderr, "   plmax-60ms %d bytes (%0.0f kbps)",
            maxStreamLen60,
            maxStreamLen60 * 8 / 60.);
    }
    if(doTransCoding)
    {
        fprintf(stderr, "  transcoding rate %.0f kbps",
            (double)numTransCodingBytes * 8.0 *(sampFreqKHz) / totalsmpls);
    }

    fclose(inp);
	fclose(outp);
	WebRtcIsac_Free(ISAC_main_inst);


	exit(0);
}
