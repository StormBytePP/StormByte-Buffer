/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Buffer.
 *
 * StormByte-Buffer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * or later, as published by the Free Software Foundation.
 *
 * StormByte-Buffer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with StormByte-Buffer. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
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