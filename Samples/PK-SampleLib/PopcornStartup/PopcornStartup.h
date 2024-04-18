#pragma once

//----------------------------------------------------------------------------
// This program is the property of Persistant Studios SARL.
//
// You may not redistribute it and/or modify it under any conditions
// without written permission from Persistant Studios SARL, unless
// otherwise stated in the latest Persistant Studios Code License.
//
// See the Persistant Studios Code License for further details.
//----------------------------------------------------------------------------

#include <PK-SampleLib/PKSample.h>
#include <pk_kernel/include/kr_scheduler.h>
#include <pk_kernel/include/kr_file.h>

__PK_SAMPLE_API_BEGIN
//----------------------------------------------------------------------------

bool	PopcornStartup(bool stdOut, File::FnNewFileSystem newFileSys = null, Scheduler::FnCreateThreadPool newThreadPool = null);
bool	PopcornShutdown();

//----------------------------------------------------------------------------
__PK_SAMPLE_API_END
