#include "adts.h"
#include "kmp.h"
#include "avreader.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
	int IsLoop;
	TToolAvCodec codec;
	const char *pFilePath;
	TToolDataCallback callback;
	void *pCbOpaque;
	int nG711FrameLen;
	int IsWithoutAdts;

	KMP kmp;
	const unsigned char *pFileData;
	int nFileLen;
	TToolAvType avType;

	pthread_t audioThreadId;
	pthread_t videoThreadId;
	int isStop;

	int nOffset;
	int IsFinished;
	int nAACFreq;
}TToolReadPolicy;

typedef struct {
	int startPos;
	int endPos;
	int startCodeLen;
}NaluRes;

typedef struct ADTS{
        TToolADTSFixheader fix;
        TToolADTSVariableHeader var;
}ADTS;

KMP gKmp;

static inline int64_t getCurrentMilliSecond(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

static int findNalStartcode(const unsigned char *f, int fLen, NaluRes *pRes) {

	memset(pRes, 0, sizeof(NaluRes));
	int idx = 0; 
	int accidx = idx;

	while(1){
		idx = FindPatternIndex(&gKmp, f + accidx, fLen - accidx);
		if (idx < 0) {
			return -1;
		}
		idx += accidx;
		if (idx+4 >= fLen) { //0x00 0x00 0x00 0x01
			return -2;
		}

		if (f[idx+2] == 0x01) {
			pRes->startCodeLen = 3;
			pRes->startPos = idx;
			accidx = (idx + 3);
			break;
		} else if (f[idx+2] == 0x00 && f[idx+3] == 0x01) {
			pRes->startCodeLen = 4;
			pRes->startPos = idx;
			accidx = (idx + 4);
			break;
		} else {
			accidx = (idx + 2);
		}
	}

	while(1){
		idx = FindPatternIndex(&gKmp, f + accidx, fLen - accidx);
		if (idx < 0 || idx+4 >= fLen) {
			pRes->endPos = fLen;
			return 0;
		}
		idx += accidx;

		//fmt.Println("-------->", idx)
		if (f[idx+2] == 0x01) {
			pRes->endPos = idx;
			accidx = (idx + 3);
			break;
		} else if (f[idx+2] == 0x00 && f[idx+3] == 0x01) {
			pRes->endPos = idx;
			accidx = (idx + 4);
			break;
		} else {
			accidx = (idx + 2);
		}
	}
	return 0;

}

static int getFileAndLength(const char *_pFname, FILE **_pFile, int *_pLen)
{
        FILE * f = fopen(_pFname, "r");
        if ( f == NULL ) {
                return -1;
        }
        *_pFile = f;
        fseek(f, 0, SEEK_END);
        long nLen = ftell(f);
        fseek(f, 0, SEEK_SET);
        *_pLen = (int)nLen;
        return 0;
}

static int readFileToBuf(const char * _pFilename, char ** _pBuf, int *_pLen)
{
        int ret;
        FILE * pFile;
        int nLen = 0;
        ret = getFileAndLength(_pFilename, &pFile, &nLen);
        if (ret != 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                return -1;
        }
        char *pData = malloc(nLen);
        assert(pData != NULL);
        ret = fread(pData, 1, nLen, pFile);
        if (ret <= 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                fclose(pFile);
                free(pData);
                return -2;
        }
        fclose(pFile);
        *_pBuf = pData;
        *_pLen = nLen;
        return 0;
}

static int getH264Frame(TToolReadPolicy *pCtx, const unsigned char **pFrame, int *pFrameLen, int *pIsKeyFrame)
{
	const unsigned char *frames = (const unsigned char *)pCtx->pFileData + pCtx->nOffset;
	NaluRes res;
	const unsigned char *retP = NULL;
	int retLen = 0;
	if (pCtx->IsFinished) {
		return -10;
	}

	int ret = 0;
	while(1){
		ret = findNalStartcode(frames, pCtx->nFileLen - pCtx->nOffset, &res);
		if (ret == 0) {
			pCtx->nOffset += res.endPos;

			const unsigned char *f = &frames[res.startPos];
			if (retP == NULL)
				retP = f;
			retLen += (res.endPos - res.startPos);

			int type = f[res.startCodeLen] & 0x1F;
			if (type == 1 || type == 5) {
				if (type == 5) {
					*pIsKeyFrame = 1;
				}
				*pFrame = retP;
				*pFrameLen = retLen;
				return 0;
			}

			frames = (const unsigned char *)pCtx->pFileData + pCtx->nOffset;
		} else {
			if (pCtx->IsLoop) {
				pCtx->nOffset = 0;
				continue;
			} else {
				pCtx->IsFinished = 1;
				break;
			}
		}
	}
	return -64;
}


enum HEVCNALUnitType {
        HEVC_NAL_TRAIL_N    = 0,
        HEVC_NAL_TRAIL_R    = 1,
        HEVC_NAL_TSA_N      = 2,
        HEVC_NAL_TSA_R      = 3,
        HEVC_NAL_STSA_N     = 4,
        HEVC_NAL_STSA_R     = 5,
        HEVC_NAL_RADL_N     = 6,
        HEVC_NAL_RADL_R     = 7,
        HEVC_NAL_RASL_N     = 8,
        HEVC_NAL_RASL_R     = 9,
        HEVC_NAL_VCL_N10    = 10,
        HEVC_NAL_VCL_R11    = 11,
        HEVC_NAL_VCL_N12    = 12,
        HEVC_NAL_VCL_R13    = 13,
        HEVC_NAL_VCL_N14    = 14,
        HEVC_NAL_VCL_R15    = 15,
        HEVC_NAL_BLA_W_LP   = 16,
        HEVC_NAL_BLA_W_RADL = 17,
        HEVC_NAL_BLA_N_LP   = 18,
        HEVC_NAL_IDR_W_RADL = 19,
        HEVC_NAL_IDR_N_LP   = 20,
        HEVC_NAL_CRA_NUT    = 21,
        HEVC_NAL_IRAP_VCL22 = 22,
        HEVC_NAL_IRAP_VCL23 = 23,
        HEVC_NAL_RSV_VCL24  = 24,
        HEVC_NAL_RSV_VCL25  = 25,
        HEVC_NAL_RSV_VCL26  = 26,
        HEVC_NAL_RSV_VCL27  = 27,
        HEVC_NAL_RSV_VCL28  = 28,
        HEVC_NAL_RSV_VCL29  = 29,
        HEVC_NAL_RSV_VCL30  = 30,
        HEVC_NAL_RSV_VCL31  = 31,
        HEVC_NAL_VPS        = 32,
        HEVC_NAL_SPS        = 33,
        HEVC_NAL_PPS        = 34,
        HEVC_NAL_AUD        = 35,
        HEVC_NAL_EOS_NUT    = 36,
        HEVC_NAL_EOB_NUT    = 37,
        HEVC_NAL_FD_NUT     = 38,
        HEVC_NAL_SEI_PREFIX = 39,
        HEVC_NAL_SEI_SUFFIX = 40,
};
enum HevcType {
        HEVC_META = 0,
        HEVC_I = 1,
        HEVC_B =2
};
static int is_h265_picture(int t)
{
        switch (t) {
                case HEVC_NAL_VPS:
                case HEVC_NAL_SPS:
                case HEVC_NAL_PPS:
                case HEVC_NAL_SEI_PREFIX:
                        return HEVC_META;
                case HEVC_NAL_IDR_W_RADL:
                case HEVC_NAL_CRA_NUT:
                        return HEVC_I;
                case HEVC_NAL_TRAIL_N:
                case HEVC_NAL_TRAIL_R:
                case HEVC_NAL_RASL_N:
                case HEVC_NAL_RASL_R:
                        return HEVC_B;
                default:
                        return -1;
        }
}

static int getH265Frame(TToolReadPolicy *pCtx, const unsigned char **pFrame, int *pFrameLen, int *pIsKeyFrame)
{
	const unsigned char *frames = (const unsigned char *)pCtx->pFileData + pCtx->nOffset;
	NaluRes res;
	const unsigned char *retP = NULL;
	int retLen = 0;
	if (pCtx->IsFinished) {
		return -10;
	}

	int ret = 0;
	while(1){
		ret = findNalStartcode(frames, pCtx->nFileLen - pCtx->nOffset, &res);
		if (ret == 0) {
			pCtx->nOffset += res.endPos;

			const unsigned char *f = &frames[res.startPos];
			if (retP == NULL)
				retP = f;
			retLen += (res.endPos - res.startPos);

			int hevctype = f[res.startCodeLen] & 0x7E;
			hevctype = (hevctype >> 1);
			int type = is_h265_picture(hevctype);
                        if (type == -1) {
                                printf("unknown type:%d\n", hevctype);
                                continue;
                        }

			if (type == HEVC_I || type == HEVC_I) {
				if (type == HEVC_I) {
					*pIsKeyFrame = 1;
				}
				*pFrame = retP;
				*pFrameLen = retLen;
				return 0;
			}

			frames = (const unsigned char *)pCtx->pFileData + pCtx->nOffset;
		} else {
			if (pCtx->IsLoop) {
				pCtx->nOffset = 0;
				continue;
			} else {
				pCtx->IsFinished = 1;
				break;
			}
		}
	}
	return -65;
}

int getG711Frame(TToolReadPolicy *pCtx, const unsigned char **pFrame, int *pFrameLen) {

	int rlen = pCtx->nG711FrameLen;

	if (pCtx->nOffset + rlen > pCtx->nFileLen) {
		if (pCtx->IsLoop) {
			pCtx->nOffset = 0;
		} else {
			return -10;
		}
	}
	*pFrame = pCtx->pFileData + pCtx->nOffset;
	*pFrameLen = rlen;
	pCtx->nOffset += rlen;

	return 0;
}

static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
int getAACFrame(TToolReadPolicy *pCtx, const unsigned char **pFrame, int *pFrameLen) {

	ADTS adts;
	int retryCnt = 0;
RETRY:
        if(pCtx->nOffset + 7 <= pCtx->nFileLen) {
                TToolParseAdtsfixedHeader((unsigned char *)(pCtx->pFileData + pCtx->nOffset), &adts.fix);
                int hlen = adts.fix.protection_absent == 1 ? 7 : 9;
                TToolParseAdtsVariableHeader((unsigned char *)(pCtx->pFileData + pCtx->nOffset), &adts.var);

                if (pCtx->nOffset + hlen + adts.var.aac_frame_length <= pCtx->nFileLen) {

			if (pCtx->IsWithoutAdts) {
				*pFrame = pCtx->pFileData + pCtx->nOffset + hlen;
				*pFrameLen = adts.var.aac_frame_length - hlen;;
			} else {
				*pFrame = pCtx->pFileData + pCtx->nOffset;
				*pFrameLen = adts.var.aac_frame_length;
			}
                        pCtx->nOffset += adts.var.aac_frame_length;
			if (pCtx->nAACFreq == 0)
				pCtx->nAACFreq = aacfreq[adts.fix.sampling_frequency_index];
			return 0;
                } else {
			if (pCtx->IsLoop) {
				pCtx->nOffset = 0;
				if (retryCnt == 0)
					goto RETRY;
			} else {
				return -10;
			}
                }
        }
	if (pCtx->IsLoop) {
		pCtx->nOffset = 0;
		if (retryCnt == 0)
			goto RETRY;
	}
	return -10;
}

static int getOneFrame(TToolReadPolicy *pCtx, const unsigned char **pFrame, int *pFrameLen, int *pIsKeyFrame) {

	*pIsKeyFrame = 0;
	switch (pCtx->codec) {
		case TTOOL_AUDIO_AAC:
			return getAACFrame(pCtx, pFrame, pFrameLen);
		case TTOOL_AUDIO_G711:
			return getG711Frame(pCtx, pFrame, pFrameLen);
		case TTOOL_VIDEO_H264:
			return getH264Frame(pCtx, pFrame, pFrameLen, pIsKeyFrame);
		case TTOOL_VIDEO_H265:
			return getH265Frame(pCtx, pFrame, pFrameLen, pIsKeyFrame);
		default:
			return -1;
	}
}

void *startFixStepIntervalThread(void *opaque, TToolAvType avType, int nInterval)
{
        TToolReadPolicy *pCtx = (TToolReadPolicy*)opaque;

        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextFrameTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;

	const unsigned char *pFrame;
	int nFrameLen;
	int nIsKeyFrame;
	
        while (!pCtx->isStop) {
		getOneFrame(pCtx, &pFrame, &nFrameLen, &nIsKeyFrame);
		// pCtx->nRolloverTestBase+nNextFrameTime-nSysTimeBase, nIsKeyFrame);
                int cbRet = pCtx->callback(pCtx->pCbOpaque, (void *)pFrame, nFrameLen, avType, nNextFrameTime-nSysTimeBase, nIsKeyFrame);
                if (cbRet != 0) {
			printf("video callback fail\n");
                }

		nNextFrameTime += nInterval;
        
                int64_t nSleepTime = 0;
                nSleepTime = (nNextFrameTime - nNow - 1) * 1000;
                if (nSleepTime > 0) {
                        //printf("sleeptime:%lld\n", nSleepTime);
                        if (nSleepTime > nInterval * 1000) {
                                printf("abnormal time diff:%lld", nSleepTime);
                        }
                        usleep(nSleepTime);
                }
                nNow = getCurrentMilliSecond();
        }

        return NULL;
}

void * startH264Thread(void *opaque) {
	return startFixStepIntervalThread(opaque, TTOOL_VIDEO_TYPE, 40);
}

void * startH265Thread(void *opaque) {
	return startFixStepIntervalThread(opaque, TTOOL_VIDEO_TYPE, 40);
}

void *startG711Thread(void *opaque) {
        TToolReadPolicy *pCtx = (TToolReadPolicy*)opaque;
	int nInterval = pCtx->nG711FrameLen * 1000 / 8000;
	return startFixStepIntervalThread(opaque, TTOOL_AUDIO_TYPE, nInterval);
}

void *startAACThread(void *opaque) {
        TToolReadPolicy *pCtx = (TToolReadPolicy*)opaque;

        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextFrameTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;

	const unsigned char *pFrame;
	int nFrameLen;
	int nIsKeyFrame;

	int64_t aacFrameCount = 0;
	
        while (!pCtx->isStop) {
		getOneFrame(pCtx, &pFrame, &nFrameLen, &nIsKeyFrame);
                int cbRet = pCtx->callback(pCtx->pCbOpaque, (void *)pFrame, nFrameLen, TTOOL_AUDIO_TYPE, nNextFrameTime-nSysTimeBase, nIsKeyFrame);
                if (cbRet != 0) {
			printf("audio callback fail\n");
                }

		aacFrameCount++;
		int64_t d = ((1024*1000.0)/pCtx->nAACFreq) * aacFrameCount;
		nNextFrameTime = nSysTimeBase + d;
        
                int64_t nSleepTime = 0;
                nSleepTime = (nNextFrameTime - nNow - 1) * 1000;
                if (nSleepTime > 0) {
                        usleep(nSleepTime);
                }
                nNow = getCurrentMilliSecond();
        }

        return NULL;
}

int TToolStartRead(TToolReadArg *pArg, void **pHandle)
{
	if (gKmp.patternSize == 0) {
		unsigned char zz[2] = {0, 0};
		InitKmp(&gKmp, zz, 2);
	}

	TToolReadPolicy *pCtx = malloc(sizeof(TToolReadPolicy));
	if (pCtx == NULL) {
		return -7;
	}
	memset(pCtx, 0, sizeof(TToolReadPolicy));
	memcpy(pCtx, pArg, sizeof(TToolReadArg));
	if (pCtx->nG711FrameLen <=0 )
		pCtx->nG711FrameLen = 160;

	int ret = readFileToBuf(pCtx->pFilePath, (char **)&pCtx->pFileData, &pCtx->nFileLen);
        if (ret != 0) {
		free(pCtx);
                fprintf(stderr, "ReadFile: fail:%d\n", ret);
                return -8;
        } 	

	*pHandle = (void *)pCtx;
	if (pCtx->callback != NULL) {
		switch (pCtx->codec) {
			case TTOOL_AUDIO_AAC:
				pthread_create( &pCtx->audioThreadId, NULL, startAACThread, pCtx);
				return 0;
			case TTOOL_AUDIO_G711:
				pthread_create( &pCtx->audioThreadId, NULL, startG711Thread, pCtx);
				return 0;
			case TTOOL_VIDEO_H264:
				pthread_create( &pCtx->videoThreadId, NULL, startH264Thread, pCtx);
				return 0;
			case TTOOL_VIDEO_H265:
				pthread_create( &pCtx->videoThreadId, NULL, startH265Thread, pCtx);
				return 0;
			default:
				return -12;
		}
	}
	return 0;
}

int  TToolGetFrame(void *pHandle, const unsigned char **pFrame, int *nFrameLen, int *pIsKeyFrame)
{
	TToolReadPolicy *pCtx = (TToolReadPolicy *)pHandle;
	*pIsKeyFrame = 0;
	if (pCtx->callback == NULL) {
		return getOneFrame(pCtx, pFrame, nFrameLen, pIsKeyFrame);
	}
	return -9;
}

void TToolStopRead(void **pHandle)
{
	TToolReadPolicy *pCtx = (TToolReadPolicy *)(*pHandle);
	if (pCtx) {
		if (pCtx->pFileData) {
			free((void *)pCtx->pFileData);
		}
		free(pCtx);
	}
	*pHandle = NULL;
}
