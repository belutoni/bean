#pragma once

#include <Windows.h>

/// <summary>
/// Small RAII helpers for Win32 handles.
/// </summary>
namespace winpp {

	/// <summary>
	/// Move-only owner of a Win32 handle that closes it on destruction.
	/// Both nullptr and INVALID_HANDLE_VALUE are treated as "no handle",
	/// since different Win32 functions use different values to signal failure.
	/// </summary>
	/// <typeparam name="CloseHandleFunc">The function used to close the handle (e.g. CloseHandle).</typeparam>
	/// <typeparam name="InvalidHandleValue">The value an empty wrapper holds internally.</typeparam>
	template<auto CloseHandleFunc, HANDLE InvalidHandleValue = INVALID_HANDLE_VALUE>
	class UniqueHandle {
	public:
		/// <summary>
		/// Creates an empty wrapper that does not own a handle.
		/// </summary>
		UniqueHandle() noexcept = default;

		/// <summary>
		/// Takes ownership of the given handle. It will be closed when the wrapper
		/// is destroyed or reset.
		/// </summary>
		/// <param name="handle">The handle to own. May be an invalid value.</param>
		explicit UniqueHandle(HANDLE handle) noexcept : m_handle(handle) {}

		// Delete copy operations; a handle has exactly one owner.
		UniqueHandle(UniqueHandle const&) = delete;
		UniqueHandle& operator=(UniqueHandle const&) = delete;

		/// <summary>
		/// Transfers ownership from another wrapper, leaving it empty.
		/// </summary>
		/// <param name="other">The wrapper to take the handle from.</param>
		UniqueHandle(UniqueHandle&& other) noexcept : m_handle(other.m_handle) {
			other.m_handle = InvalidHandleValue;
		}

		/// <summary>
		/// Closes the currently owned handle (if any) and takes ownership
		/// from another wrapper, leaving it empty.
		/// </summary>
		/// <param name="other">The wrapper to take the handle from.</param>
		/// <returns>A reference to this wrapper.</returns>
		UniqueHandle& operator=(UniqueHandle&& other) noexcept {
			if (this != &other) {
				reset();
				m_handle = other.m_handle;
				other.m_handle = InvalidHandleValue;
			}

			return *this;
		}

		/// <summary>
		/// Returns the underlying handle without giving up ownership.
		/// The caller must not close it.
		/// </summary>
		/// <returns>The raw handle, which may be invalid.</returns>
		[[nodiscard]] HANDLE get() const noexcept {
			return m_handle;
		}

		/// <summary>
		/// Gives up ownership of the handle and returns it. The wrapper becomes
		/// empty, and the caller is now responsible for closing the handle.
		/// </summary>
		/// <returns>The raw handle, which may be invalid.</returns>
		[[nodiscard]] HANDLE release() noexcept {
			HANDLE temp{ m_handle };
			m_handle = InvalidHandleValue;

			return temp;
		}

		/// <summary>
		/// Allows the wrapper to be used in a condition, e.g. if (!handle).
		/// </summary>
		/// <returns>True if the wrapper owns a valid handle; false otherwise.</returns>
		[[nodiscard]] explicit operator bool() const noexcept {
			return valid();
		}

		/// <summary>
		/// Checks whether the wrapper owns a usable handle.
		/// </summary>
		/// <returns>True if the handle is neither nullptr nor INVALID_HANDLE_VALUE.</returns>
		[[nodiscard]] bool valid() const noexcept {
			return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
		}

		/// <summary>
		/// Closes the currently owned handle (if valid) and optionally takes
		/// ownership of a new one. Calling it with no argument just closes the handle.
		/// </summary>
		/// <param name="new_handle">The handle to own afterwards. Defaults to empty.</param>
		void reset(HANDLE new_handle = InvalidHandleValue) noexcept {
			HANDLE old = m_handle;
			m_handle = new_handle;
			if (old && old != INVALID_HANDLE_VALUE) {
				CloseHandleFunc(old);
			}
		}

		/// <summary>
		/// Closes the owned handle, if any.
		/// </summary>
		~UniqueHandle() {
			reset();
		}
	private:
		/// <summary>
		/// The owned handle, or InvalidHandleValue when the wrapper is empty.
		/// </summary>
		HANDLE m_handle{ InvalidHandleValue };
	};

	/// <summary>
	/// Owns a general kernel object handle (events, files, etc.), closed with CloseHandle.
	/// Functions like CreateEventW return nullptr on failure.
	/// </summary>
	using Handle = UniqueHandle<CloseHandle, nullptr>;

	/// <summary>
	/// Owns a change notification handle, closed with FindCloseChangeNotification.
	/// FindFirstChangeNotificationW returns INVALID_HANDLE_VALUE on failure.
	/// </summary>
	using ChangeNotificationHandle = UniqueHandle<FindCloseChangeNotification, INVALID_HANDLE_VALUE>;
}