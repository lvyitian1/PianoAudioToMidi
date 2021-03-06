#pragma once
#include "targetver.h"

#pragma warning(push, 3)
#	include <Windows.h>
#	include <CommCtrl.h>
#	include <gdiplus.h>
#pragma warning(pop)
#pragma comment(lib, "Gdiplus.lib")
#include <WindowsX.h>

#include <cassert>
#pragma warning(push, 3)
#pragma warning(disable:4244 4365 4514 4571 4625 4626 4710 4711 4774 4820 5026 5027 5045)
#	include <codecvt>
#	include <regex>
#pragma warning(pop)

#pragma warning(push, 3)
#	include <ippcore.h>
#	include <ippvm.h>
#	include <ipps.h>
#	include <ippi.h>

#	include <mkl_trans.h>
#	include <mkl_cblas.h>
#	include <mkl_spblas.h>
#	include <mkl_vml_functions.h>
#pragma warning(pop)

#pragma warning(push, 3)
#pragma warning(disable:4242 4365 4514 4571 4619 4625 4626 4668 4710 4711 4774 4820 4826 5026 5027 5031 26439 26495 28251)
#	include <boost/filesystem/operations.hpp>
#	include <boost/archive/binary_iarchive.hpp>
#	include <boost/serialization/array.hpp>
#	include <boost/align/aligned_allocator.hpp>
#	ifdef _DEBUG
#		include <boost/align/is_aligned.hpp>
#	endif
#pragma warning(pop)

extern "C"
{
#pragma warning(push, 2)
#	include <libavformat/avformat.h>
#	include <libswresample/swresample.h>
#pragma warning(pop)
}
#pragma comment(lib, "avcodec")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#	pragma comment(lib, "Bcrypt") // required by AvUtil
#pragma comment(lib, "swresample")

#pragma warning(push, 2)
#pragma warning(disable:4365 4514 4571 4625 4626 4710 4711 4774 4820 5026 5027 26434 26439 26451 26495)
#	include <Juce/AppConfig.h>
#	include <juce_dsp/juce_dsp.h>
#	include <juce_audio_basics/juce_audio_basics.h>
#pragma warning(pop)
#pragma comment(lib, "Juce")

#pragma warning(push, 0)
#pragma warning(disable:4355 4514 4571 4623 4625 4626 4701 4702 4710 4711 4714 4820 5026 5027 5039 5045 6001 6255 6294 26110 26115 26117 26444 26450 26451 26454 26495 26498 28020)
#	include <fdeep/fdeep.hpp>
#pragma warning(pop)