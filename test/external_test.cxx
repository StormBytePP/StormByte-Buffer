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
#include <StormByte/buffer/producer.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <string>

using StormByte::Buffer::DataType;
using StormByte::Buffer::ExternalBufferReader;
using StormByte::Buffer::ExternalBufferWriter;
using StormByte::Buffer::ExternalReader;
using StormByte::Buffer::ExternalWriter;
using StormByte::Buffer::FIFO;
using StormByte::Buffer::Position;
using StormByte::Buffer::Producer;

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

class CountingWriter final : public ExternalWriter {
	public:
		explicit CountingWriter(FIFO& target) noexcept : m_target(target) {}

		bool IsWritable() const noexcept override {
			return m_target.IsWritable();
		}

		std::size_t Occupied() const noexcept override {
			return m_target.Size();
		}

		bool Write(const DataType& data) noexcept override {
			return m_target.Write(0, data);
		}

		bool Write(DataType&& data) noexcept override {
			return m_target.Write(0, std::move(data));
		}

		bool Write(std::size_t count, const DataType& data) noexcept override {
			return m_target.Write(count, data);
		}

		bool Write(std::size_t count, DataType&& data) noexcept override {
			return m_target.Write(count, std::move(data));
		}

		using ExternalWriter::Write;

		void Close() noexcept override { m_target.Close(); }
		void SetError() noexcept override { m_target.SetError(); }

		PointerType Clone() const noexcept override {
			return MakePointer<CountingWriter>(m_target);
		}

		PointerType Move() noexcept override {
			return MakePointer<CountingWriter>(m_target);
		}

	private:
		FIFO& m_target;
};

// -------------------
// Reader ABI
// -------------------

int test_external_buffer_reader_polymorphic_abi() {
	const std::string fn = "test_external_buffer_reader_polymorphic_abi";
	FIFO source;
	ASSERT_TRUE(fn, source.Write("AB"));
	ExternalBufferReader adapter(source);
	ExternalReader& reader = adapter;
	ASSERT_EQUAL(fn, static_cast<std::size_t>(2), reader.AvailableBytes());
	ASSERT_FALSE(fn, reader.Empty());
	ASSERT_FALSE(fn, reader.EoF());
	ASSERT_TRUE(fn, reader.IsReadable());
	DataType peek;
	ASSERT_TRUE(fn, reader.Peek(1, peek));
	DataType read;
	ASSERT_TRUE(fn, reader.Read(1, read));
	reader.Seek(0, Position::Absolute);
	DataType extracted;
	ASSERT_TRUE(fn, reader.Extract(1, extracted));
	reader.Clean();
	source.Close();
	DataType remaining;
	reader.ReadUntilEoF(remaining);
	DataType none;
	reader.ExtractUntilEoF(none);
	auto clone = reader.Clone();
	ASSERT_TRUE(fn, static_cast<bool>(clone));
	auto moved = reader.Move();
	ASSERT_TRUE(fn, static_cast<bool>(moved));
	RETURN_TEST(fn, 0);
}

int test_external_reader_default_interface_abi() {
	const std::string fn = "test_external_reader_default_interface_abi";
	DefaultExternalReader adapter;
	ExternalReader& reader = adapter;
	reader.Seek(0, Position::Absolute);
	reader.Clean();
	RETURN_TEST(fn, 0);
}

// -------------------
// Writer ABI
// -------------------

int test_external_buffer_writer_polymorphic_abi() {
	const std::string fn = "test_external_buffer_writer_polymorphic_abi";
	FIFO target;
	ExternalBufferWriter adapter(target);
	ExternalWriter& writer = adapter;
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), writer.Occupied());
	DataType copy {std::byte{'A'}};
	DataType moved {std::byte{'B'}};
	ASSERT_TRUE(fn, writer.Write(copy));
	ASSERT_TRUE(fn, writer.Write(std::move(moved)));
	ASSERT_TRUE(fn, writer.Write(1, copy));
	DataType counted_move {std::byte{'C'}};
	ASSERT_TRUE(fn, writer.Write(1, std::move(counted_move)));
	ASSERT_TRUE(fn, writer.IsWritable());
	ASSERT_EQUAL(fn, target.Size(), writer.Occupied());
	auto clone = writer.Clone();
	ASSERT_TRUE(fn, static_cast<bool>(clone));
	auto moved_adapter = writer.Move();
	ASSERT_TRUE(fn, static_cast<bool>(moved_adapter));
	writer.Close();
	ASSERT_FALSE(fn, writer.IsWritable());
	ASSERT_EQUAL(fn, target.Size(), writer.Occupied());
	ExternalBufferWriter error_adapter(target);
	ExternalWriter& error_writer = error_adapter;
	error_writer.SetError();
	ASSERT_FALSE(fn, error_writer.IsWritable());
	RETURN_TEST(fn, 0);
}

// -------------------
// Occupied
// -------------------

int test_occupied_empty_is_zero() {
	const std::string fn = "test_occupied_empty_is_zero";
	FIFO target;
	ExternalBufferWriter adapter(target);
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), adapter.Occupied());
	ASSERT_EQUAL(fn, target.Size(), adapter.Occupied());
	RETURN_TEST(fn, 0);
}

int test_occupied_tracks_fifo_size() {
	const std::string fn = "test_occupied_tracks_fifo_size";
	FIFO target;
	ExternalBufferWriter adapter(target);
	ExternalWriter& writer = adapter;
	ASSERT_TRUE(fn, writer.Write("HELLO"));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(5), writer.Occupied());
	ASSERT_EQUAL(fn, target.Size(), writer.Occupied());
	ASSERT_TRUE(fn, writer.Write("!!"));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(7), writer.Occupied());
	ASSERT_EQUAL(fn, target.Size(), writer.Occupied());
	RETURN_TEST(fn, 0);
}

int test_occupied_leaf_matches_store() {
	const std::string fn = "test_occupied_leaf_matches_store";
	FIFO target;
	CountingWriter leaf(target);
	ExternalWriter& writer = leaf;
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), writer.Occupied());
	ASSERT_TRUE(fn, writer.Write("ABC"));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(3), writer.Occupied());
	ASSERT_EQUAL(fn, target.Size(), writer.Occupied());
	RETURN_TEST(fn, 0);
}

int test_occupied_producer() {
	const std::string fn = "test_occupied_producer";
	Producer producer;
	ExternalBufferWriter adapter(producer);
	ExternalWriter& writer = adapter;
	ASSERT_EQUAL(fn, static_cast<std::size_t>(0), writer.Occupied());
	ASSERT_EQUAL(fn, producer.Size(), writer.Occupied());
	ASSERT_TRUE(fn, writer.Write("XYZ"));
	ASSERT_EQUAL(fn, static_cast<std::size_t>(3), writer.Occupied());
	ASSERT_EQUAL(fn, producer.Size(), writer.Occupied());
	RETURN_TEST(fn, 0);
}

int test_occupied_survives_close() {
	const std::string fn = "test_occupied_survives_close";
	FIFO target;
	ExternalBufferWriter adapter(target);
	ExternalWriter& writer = adapter;
	ASSERT_TRUE(fn, writer.Write("KEEP"));
	adapter.Close();
	ASSERT_FALSE(fn, adapter.IsWritable());
	ASSERT_EQUAL(fn, static_cast<std::size_t>(4), adapter.Occupied());
	ASSERT_EQUAL(fn, target.Size(), adapter.Occupied());
	RETURN_TEST(fn, 0);
}

int main() {
	int result = 0;

	// -------------------
	// Reader ABI
	// -------------------
	result += test_external_buffer_reader_polymorphic_abi();
	result += test_external_reader_default_interface_abi();

	// -------------------
	// Writer ABI
	// -------------------
	result += test_external_buffer_writer_polymorphic_abi();

	// -------------------
	// Occupied
	// -------------------
	result += test_occupied_empty_is_zero();
	result += test_occupied_tracks_fifo_size();
	result += test_occupied_leaf_matches_store();
	result += test_occupied_producer();
	result += test_occupied_survives_close();

	if (result == 0) {
		std::cout << "External tests passed!" << std::endl;
	} else {
		std::cout << result << " External tests failed." << std::endl;
	}

	return result;
}
