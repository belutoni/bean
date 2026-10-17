#pragma once

#include <Windows.h>

namespace winpp {

	template<auto CloseHandleFunc, HANDLE InvalidHandleValue = INVALID_HANDLE_VALUE>
	class UniqueHandle {
	public:
		UniqueHandle() noexcept = default;
		explicit UniqueHandle(HANDLE handle) noexcept : m_handle(handle) {}

		UniqueHandle(UniqueHandle const&) = delete;
		UniqueHandle& operator=(UniqueHandle const&) = delete;

		UniqueHandle(UniqueHandle&& other) noexcept : m_handle(other.m_handle) {
			other.m_handle = InvalidHandleValue;
		}

		UniqueHandle& operator=(UniqueHandle&& other) noexcept {
			if (this != &other) {
				reset();
				m_handle = other.m_handle;
				other.m_handle = InvalidHandleValue;
			}

			return *this;
		}

		[[nodiscard]] HANDLE get() const noexcept { 
			return m_handle; 
		}

		[[nodiscard]] HANDLE release() noexcept {
			HANDLE temp{ m_handle };
			m_handle = InvalidHandleValue;

			return temp;
		}

		[[nodiscard]] explicit operator bool() const noexcept {
			return valid();
		}

		[[nodiscard]] bool valid() const noexcept {
			return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
		}

		void reset(HANDLE new_handle = InvalidHandleValue) noexcept {
			HANDLE old = m_handle;
			m_handle = new_handle;
			if (old && old != INVALID_HANDLE_VALUE) {
				CloseHandleFunc(old);
			}
		}

		~UniqueHandle() {
			reset();
		}
	private:
		HANDLE m_handle{ InvalidHandleValue };
	};

	using Handle					= UniqueHandle<CloseHandle, nullptr>;
	using ChangeNotificationHandle	= UniqueHandle<FindCloseChangeNotification, INVALID_HANDLE_VALUE>;
}