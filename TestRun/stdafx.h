#pragma once
#include "targetver.h"

#include <vld.h>

#pragma warning(push, 3)
#pragma warning(disable:5039)
#	include <Windows.h>
#	include <CommCtrl.h>
#pragma warning(pop)
#include <WindowsX.h>

#pragma warning(push, 3)
#pragma warning(disable:4365 4514 4571 4599 4625 4626 4774 4820 5026 5027)
#	include <numeric>
#	include <regex>
#	include <sstream>
#pragma warning(pop)