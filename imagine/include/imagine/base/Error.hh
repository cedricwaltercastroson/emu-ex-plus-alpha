#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <system_error>
#include <exception>

namespace IG
{

class ErrorCode : public std::error_code
{
public:
	ErrorCode() = default;
	ErrorCode(int code);
};

class Error final: public std::exception
{
public:
	ErrorCode code{};

	Error() = default;
	Error(ErrorCode code): code{code} {}
	const char* what() const noexcept final { return ""; }
};

}
