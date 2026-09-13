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

#include <StormByte/buffer/external.hxx>
#include <StormByte/buffer/fifo.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>

using StormByte::Buffer::DataType;
using StormByte::Buffer::ExternalBufferReader;
using StormByte::Buffer::ExternalBufferWriter;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalWriter;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;

class DefaultExternalReader final : public ExternalReader {
	public:
		std::size_t AvailableBytes() const noexcept override { return 0; }
		bool Empty() const noexcept override { return true; }
		bool EoF() const noexcept override { return true; }
		bool IsReadable() const noexcept override { return true; }
		bool Read(std::size_t, DataType&) const noexcept override { return false; }
		bool Extract(std::size_t, DataType&) noexcept override { return false; }
		bool Peek(std::size_t, DataType&) const noexcept override { return false; }
		void ReadUntilEoF(DataType&) const noexcept override {}
		void ExtractUntilEoF(DataType&) noexcept override {}
		PointerType Clone() const noexcept override { return MakePointer<DefaultExternalReader>(); }
		PointerType Move() noexcept override { return MakePointer<DefaultExternalReader>(); }
};

int test_external_buffer_reader_polymorphic_abi() {
	FIFO source;
	ASSERT_TRUE("write source", source.Write("AB"));
	ExternalBufferReader adapter(source);
	ExternalReader& reader = adapter;
	ASSERT_EQUAL("available", reader.AvailableBytes(), static_cast<std::size_t>(2));
	ASSERT_FALSE("empty", reader.Empty());
	ASSERT_FALSE("eof", reader.EoF());
	ASSERT_TRUE("readable", reader.IsReadable());
	DataType peek;
	ASSERT_TRUE("peek", reader.Peek(1, peek));
	DataType read;
	ASSERT_TRUE("read", reader.Read(1, read));
	reader.Seek(0, Position::Absolute);
	DataType extracted;
	ASSERT_TRUE("extract", reader.Extract(1, extracted));
	reader.Clean();
	source.Close();
	DataType remaining;
	reader.ReadUntilEoF(remaining);
	DataType none;
	reader.ExtractUntilEoF(none);
	auto clone = reader.Clone();
	ASSERT_TRUE("clone", static_cast<bool>(clone));
	auto moved = reader.Move();
	ASSERT_TRUE("move", static_cast<bool>(moved));
	RETURN_TEST("test_external_buffer_reader_polymorphic_abi", 0);
}

int test_external_reader_default_interface_abi() {
	DefaultExternalReader adapter;
	ExternalReader& reader = adapter;
	reader.Seek(0, Position::Absolute);
	reader.Clean();
	RETURN_TEST("test_external_reader_default_interface_abi", 0);
}

int test_external_buffer_writer_polymorphic_abi() {
	FIFO target;
	ExternalBufferWriter adapter(target);
	ExternalWriter& writer = adapter;
	DataType copy {std::byte{'A'}};
	DataType moved {std::byte{'B'}};
	ASSERT_TRUE("write copy", writer.Write(copy));
	ASSERT_TRUE("write move", writer.Write(std::move(moved)));
	ASSERT_TRUE("write counted copy", writer.Write(1, copy));
	DataType counted_move {std::byte{'C'}};
	ASSERT_TRUE("write counted move", writer.Write(1, std::move(counted_move)));
	ASSERT_TRUE("writable", writer.IsWritable());
	auto clone = writer.Clone();
	ASSERT_TRUE("clone", static_cast<bool>(clone));
	auto moved_adapter = writer.Move();
	ASSERT_TRUE("move", static_cast<bool>(moved_adapter));
	writer.Close();
	ASSERT_FALSE("closed", writer.IsWritable());
	ExternalBufferWriter error_adapter(target);
	ExternalWriter& error_writer = error_adapter;
	error_writer.SetError();
	ASSERT_FALSE("error", error_writer.IsWritable());
	RETURN_TEST("test_external_buffer_writer_polymorphic_abi", 0);
}

int main() {
	int result = 0;
	result += test_external_buffer_reader_polymorphic_abi();
	result += test_external_reader_default_interface_abi();
	result += test_external_buffer_writer_polymorphic_abi();
	if (result == 0) {
		std::cout << "External tests passed!" << std::endl;
	} else {
		std::cout << result << " External tests failed." << std::endl;
	}
	return result;
}