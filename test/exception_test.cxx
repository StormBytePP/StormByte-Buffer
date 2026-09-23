/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Buffer source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte-Logger tree and
 * the rest of the StormByte suite it vendors), which remains under its own
 * license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#include <StormByte/buffer/exception.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <string>

using StormByte::Buffer::Error;
using StormByte::Buffer::Exception;
using StormByte::Buffer::ReadError;
using StormByte::Buffer::WriteError;

int test_buffer_exception_message() {
	Exception exception(std::string("message"));
	ASSERT_EQUAL("message", std::string("StormByte::Buffer: message"), std::string(exception.what()));
	RETURN_TEST("test_buffer_exception_message", 0);
}

int test_buffer_exception_format() {
	Exception exception("value is {}", 42);
	ASSERT_EQUAL("format", std::string("StormByte::Buffer: value is 42"), std::string(exception.what()));
	RETURN_TEST("test_buffer_exception_format", 0);
}

int test_buffer_exception_component() {
	Exception exception(StormByte::Component{"Buffer::FIFO"}, "failed with code {}", 7);
	ASSERT_EQUAL("component", std::string("StormByte::Buffer::FIFO: failed with code 7"), std::string(exception.what()));
	RETURN_TEST("test_buffer_exception_component", 0);
}

int test_buffer_error_message() {
	Error exception(std::string("message"));
	ASSERT_EQUAL("error", std::string("StormByte::Buffer: message"), std::string(exception.what()));
	RETURN_TEST("test_buffer_error_message", 0);
}

int test_read_error_message() {
	ReadError exception("read {}", "failed");
	ASSERT_EQUAL("read error", std::string("StormByte::Buffer::ReadError: read failed"), std::string(exception.what()));
	RETURN_TEST("test_read_error_message", 0);
}

int test_write_error_message() {
	WriteError exception(std::string("write failed"));
	ASSERT_EQUAL("write error", std::string("StormByte::Buffer::WriteError: write failed"), std::string(exception.what()));
	RETURN_TEST("test_write_error_message", 0);
}

int main() {
	int result = 0;
	result += test_buffer_exception_message();
	result += test_buffer_exception_format();
	result += test_buffer_exception_component();
	result += test_buffer_error_message();
	result += test_read_error_message();
	result += test_write_error_message();
	if (result == 0) {
		std::cout << "Exception tests passed!" << std::endl;
	} else {
		std::cout << result << " Exception tests failed." << std::endl;
	}

	return result;
}
