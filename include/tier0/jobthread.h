#ifndef JOBTHREAD_H
#define JOBTHREAD_H

#define JT_JOB_GROUP_BASE_ID 0x3000

typedef uint32_t JobID_t;
typedef uint8_t JobTypeID_t;
typedef uint32_t JobAffinity_t;

typedef bool(*JobHelpCallback_t)(__int64, _DWORD*, __int64, _QWORD*);

enum JobPriority_e : uint32_t
{
};

struct JobFifoLock_s
{
	int id;
	int depth;
	short tls[64];
};

struct JobContext_s
{
	void* callbackArg;  // Argument to job callback function.
	void* callbackFunc; // Job callback function.
	JobTypeID_t jobTypeId;
	bool field_11;
	__int16 field_12;
	int field_14;
	JobID_t jobId;
	int field_1C;
	int field_20;
	int field_24; // Bit fields?
	__int64 field_28;
	__int64 unknownMask;
	__int64 unknownInt;
};

typedef struct JobUserData_s
{
	JobUserData_s(int32_t si)
	{
		data.sint = si;
	}
	JobUserData_s(uint32_t ui)
	{
		data.uint = ui;
	}
	JobUserData_s(int64_t si)
	{
		data.sint = si;
	}
	JobUserData_s(uint64_t ui)
	{
		data.uint = ui;
	}
	JobUserData_s(double sa)
	{
		data.scal = sa;
	}
	JobUserData_s(void* pt)
	{
		data.ptr = pt;
	}

	union
	{
		int64_t sint;
		uint64_t uint;
		double scal;
		void* ptr;
	} data;
} JobUserData_t;

// Array size = 2048*sizeof(JobContext_s)
inline JobContext_s* job_JT_Context = nullptr;

extern bool JT_IsJobDone(const JobID_t jobId);
extern JobID_t JTGuts_AddJob(JobTypeID_t jobTypeId, JobID_t jobId, void* callbackFunc, void* callbackArg);
extern JobID_t JT_GetCurrentJob();


inline void(*JT_ParallelCall)(void);
inline void*(*JT_HelpWithAnything)(bool bShouldLoadPak);

inline bool(*JT_HelpWithJobTypes)(JobHelpCallback_t, JobUserData_t userData, __int64 a3, __int64 a4);
inline __int64(*JT_HelpWithJobTypesOrSleep)(JobHelpCallback_t, JobUserData_t userData, __int64 a3, __int64 a4, volatile signed __int64* a5, char a6);
inline __int64(*JT_WaitForJobAndOnlyHelpWithJobTypes)(JobID_t, uint64_t unkMask1, uint64_t unkMask2);

inline bool(*JT_AcquireFifoLock)(struct JobFifoLock_s* pFifo);
inline void(*JT_ReleaseFifoLock)(struct JobFifoLock_s* pFifo);

inline JobID_t(*JT_BeginJobGroup)(const JobID_t jobId);
inline void(*JT_EndJobGroup)(const JobID_t jobId);

inline unsigned int (*JT_AllocateJob)(); // Returns an index to the 'job_JT_Context' array
inline JobID_t(*JTGuts_AddJob_Internal)(JobTypeID_t jobTypeId, JobID_t jobId, void* callbackfunc, void* callbackArg, int jobIndex, JobContext_s* context);

#endif // JOBTHREAD_H
