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

#include <StormByte/buffer/sink.hxx>
#include <StormByte/test_handlers.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using StormByte::Buffer::Sink;

/**
 * @brief Tests default construction of Sink (zero buckets, EoF false, Ready false).
 * @return 0 on success.
 */
int test_sink_default_constructor() {
	Sink<int> sink;
	ASSERT_FALSE("test_sink_default_constructor eof", sink.EoF());
	ASSERT_FALSE("test_sink_default_constructor ready", sink.Ready());
	ASSERT_FALSE("test_sink_default_constructor draining", sink.Draining());
	ASSERT_EQUAL("test_sink_default_constructor missing key capacity", static_cast<std::size_t>(0), sink.Capacity(42));
	ASSERT_EQUAL("test_sink_default_constructor missing key size", static_cast<std::size_t>(0), sink.Size(42));
	ASSERT_FALSE("test_sink_default_constructor missing key full", sink.Full(42));

	RETURN_TEST("test_sink_default_constructor", 0);
}

/**
 * @brief Tests Bind and Push/Pop flow across multiple integer key buckets.
 * @return 0 on success.
 */
int test_sink_bind_and_push_pop() {
	Sink<std::shared_ptr<std::string>> producer;
	Sink<std::shared_ptr<std::string>> consumer;

	producer.Bind(10, consumer);
	producer.Bind(20, consumer);

	producer.Capacity(10, 5);
	ASSERT_EQUAL("test_sink_bind_and_push_pop capacity key 10", static_cast<std::size_t>(5), producer.Capacity(10));

	producer.Push(10, std::make_shared<std::string>("String-10"));
	producer.Push(20, std::make_shared<std::string>("String-20"));

	ASSERT_EQUAL("test_sink_bind_and_push_pop size key 10", static_cast<std::size_t>(1), consumer.Size(10));
	ASSERT_EQUAL("test_sink_bind_and_push_pop size key 20", static_cast<std::size_t>(1), consumer.Size(20));
	ASSERT_TRUE("test_sink_bind_and_push_pop consumer ready", consumer.Ready());

	auto item1 = consumer.Pop();
	auto item2 = consumer.Pop();

	ASSERT_TRUE("test_sink_bind_and_push_pop item1 valid", static_cast<bool>(item1));
	ASSERT_TRUE("test_sink_bind_and_push_pop item2 valid", static_cast<bool>(item2));

	producer.Eof();
	ASSERT_TRUE("test_sink_bind_and_push_pop consumer eof", consumer.EoF());

	RETURN_TEST("test_sink_bind_and_push_pop", 0);
}

/**
 * @brief Tests Sink Drain mode where Push to un-bound keys discards items without waiting.
 * @return 0 on success.
 */
int test_sink_drain_mode() {
	Sink<int> producer;
	producer.Drain();

	ASSERT_TRUE("test_sink_drain_mode draining", producer.Draining());

	producer.Push(100, 9999);
	ASSERT_EQUAL("test_sink_drain_mode size 0 for un-bound key", static_cast<std::size_t>(0), producer.Size(100));

	Sink<int> consumer;
	producer.Bind(100, consumer);

	producer.Push(100, 8888);
	ASSERT_EQUAL("test_sink_drain_mode size 1 for bound key", static_cast<std::size_t>(1), consumer.Size(100));
	ASSERT_EQUAL("test_sink_drain_mode pop value", 8888, consumer.Pop());

	RETURN_TEST("test_sink_drain_mode", 0);
}

/**
 * @brief Tests Bind(Sink& consumer) sharing all existing hoppers.
 * @return 0 on success.
 */
int test_sink_bind_all_hoppers() {
	Sink<int> producer;
	Sink<int> consumer;

	Sink<int> dummy_consumer;
	producer.Bind(1, dummy_consumer);
	producer.Bind(2, dummy_consumer);

	producer.Bind(consumer);

	producer.Push(1, 100);
	producer.Push(2, 200);

	ASSERT_EQUAL("test_sink_bind_all_hoppers consumer size key 1", static_cast<std::size_t>(1), consumer.Size(1));
	ASSERT_EQUAL("test_sink_bind_all_hoppers consumer size key 2", static_cast<std::size_t>(1), consumer.Size(2));

	int val1 = consumer.Pop();
	int val2 = consumer.Pop();

	bool correct_set = (val1 == 100 && val2 == 200) || (val1 == 200 && val2 == 100);
	ASSERT_TRUE("test_sink_bind_all_hoppers popped values", correct_set);

	RETURN_TEST("test_sink_bind_all_hoppers", 0);
}

/**
 * @brief Tests Bind after Eof where newly created/bound hoppers are born with EoF = true.
 * @return 0 on success.
 */
int test_sink_bind_after_eof() {
	Sink<int> producer;
	Sink<int> consumer;

	producer.Eof();
	ASSERT_TRUE("test_sink_bind_after_eof producer eof with zero buckets", producer.EoF());

	producer.Bind(50, consumer);
	ASSERT_TRUE("test_sink_bind_after_eof consumer born eof", consumer.EoF());
	ASSERT_TRUE("test_sink_bind_after_eof consumer ready on eof", consumer.Ready());

	RETURN_TEST("test_sink_bind_after_eof", 0);
}

/**
 * @brief Tests Pop with custom Select chooser callback.
 * @return 0 on success.
 */
int test_sink_pop_custom_select() {
	Sink<int> producer;
	Sink<int> consumer;

	producer.Bind(1, consumer);
	producer.Bind(2, consumer);

	producer.Push(1, 111);
	producer.Push(2, 222);

	auto select_second = [](std::size_t count) -> std::size_t {
		return count > 1 ? 1 : 0;
	};

	int val = consumer.Pop(select_second);
	ASSERT_EQUAL("test_sink_pop_custom_select selected bucket index 1", 222, val);

	int val_rem = consumer.Pop();
	ASSERT_EQUAL("test_sink_pop_custom_select remaining bucket index 0", 111, val_rem);

	RETURN_TEST("test_sink_pop_custom_select", 0);
}

/**
 * @brief Tests Push waiting on wiring condition variable until Bind or Eof occurs.
 * @return 0 on success.
 */
int test_sink_push_waiting_for_bind() {
	Sink<int> producer;
	Sink<int> consumer;

	std::atomic<bool> push_done{false};

	std::thread push_thread([&]() {
		producer.Push(7, 777);
		push_done.store(true, std::memory_order_release);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	ASSERT_FALSE("test_sink_push_waiting_for_bind push waiting", push_done.load(std::memory_order_acquire));

	producer.Bind(7, consumer);
	push_thread.join();

	ASSERT_TRUE("test_sink_push_waiting_for_bind push resumed", push_done.load(std::memory_order_acquire));
	ASSERT_EQUAL("test_sink_push_waiting_for_bind popped val", 777, consumer.Pop());

	RETURN_TEST("test_sink_push_waiting_for_bind", 0);
}

/**
 * @brief Tests Notify callback on Sink condition variable.
 * @return 0 on success.
 */
int test_sink_notify_condition_variable() {
	Sink<int> producer;
	Sink<int> consumer;
	producer.Bind(1, consumer);

	std::condition_variable cv;
	std::mutex m;

	consumer.Notify(cv);

	std::atomic<int> read_val{-1};
	std::thread consumer_thread([&]() {
		std::unique_lock<std::mutex> lock(m);
		cv.wait(lock, [&]() { return consumer.Ready(); });
		read_val.store(consumer.Pop(), std::memory_order_release);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	producer.Push(1, 555);
	consumer_thread.join();

	ASSERT_EQUAL("test_sink_notify_condition_variable read value", 555, read_val.load(std::memory_order_acquire));

	RETURN_TEST("test_sink_notify_condition_variable", 0);
}

/**
 * @brief Tests race condition between concurrent Bind(key) loop and Eof().
 * @return 0 on success.
 */
int test_sink_concurrent_bind_and_eof() {
	Sink<int> producer;
	Sink<int> consumer;

	std::atomic<bool> stop_binding{false};
	std::thread bind_thread([&]() {
		int key = 100;
		while (!stop_binding.load(std::memory_order_acquire)) {
			producer.Bind(key++, consumer);
			std::this_thread::yield();
		}
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	producer.Eof();
	stop_binding.store(true, std::memory_order_release);
	bind_thread.join();

	ASSERT_TRUE("test_sink_concurrent_bind_and_eof producer eof", producer.EoF());
	ASSERT_TRUE("test_sink_concurrent_bind_and_eof consumer eof", consumer.EoF());

	for (int k = 100; k < 120; ++k) {
		producer.Push(k, 9999);
		ASSERT_EQUAL("test_sink_concurrent_bind_and_eof size after push post eof", static_cast<std::size_t>(0), consumer.Size(k));
	}

	RETURN_TEST("test_sink_concurrent_bind_and_eof", 0);
}

/**
 * @brief Tests that destroying a Sink unblocks threads waiting in Push and Pop.
 * @return 0 on success.
 */
int test_sink_destructor_unblocks_waiters() {
	auto producer = std::make_unique<Sink<int>>();
	auto consumer = std::make_unique<Sink<int>>();

	std::atomic<bool> push_done{false};
	std::atomic<bool> pop_done{false};

	std::thread push_thread([&]() {
		producer->Push(10, 100);
		push_done.store(true, std::memory_order_release);
	});

	std::thread pop_thread([&]() {
		(void)consumer->Pop();
		pop_done.store(true, std::memory_order_release);
	});

	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	ASSERT_FALSE("test_sink_destructor_unblocks_waiters push blocked", push_done.load(std::memory_order_acquire));
	ASSERT_FALSE("test_sink_destructor_unblocks_waiters pop blocked", pop_done.load(std::memory_order_acquire));

	producer.reset();
	consumer.reset();

	push_thread.join();
	pop_thread.join();

	ASSERT_TRUE("test_sink_destructor_unblocks_waiters push completed on dtor", push_done.load(std::memory_order_acquire));
	ASSERT_TRUE("test_sink_destructor_unblocks_waiters pop completed on dtor", pop_done.load(std::memory_order_acquire));

	RETURN_TEST("test_sink_destructor_unblocks_waiters", 0);
}

/**
 * @brief Main entry point for Sink tests.
 * @return 0 on all tests passing, non-zero on failure.
 */
int main() {
	int failed = 0;
	failed += test_sink_default_constructor();
	failed += test_sink_bind_and_push_pop();
	failed += test_sink_drain_mode();
	failed += test_sink_bind_all_hoppers();
	failed += test_sink_bind_after_eof();
	failed += test_sink_pop_custom_select();
	failed += test_sink_push_waiting_for_bind();
	failed += test_sink_notify_condition_variable();
	failed += test_sink_concurrent_bind_and_eof();
	failed += test_sink_destructor_unblocks_waiters();

	if (failed != 0) {
		std::cerr << failed << " test(s) failed." << std::endl;
		return 1;
	}
	return 0;
}
