#pragma once

#include "event.h"
#include <process.h>

namespace c4u {

class thread
{
public:
	thread () throw() :
		m_thread (NULL),
		m_id (0),
		m_suspended (false)
	{
	}
	~thread () throw()
	{
		close ();
	}
	inline operator HANDLE () const throw()
	{
		return m_thread;
	}
	inline operator bool () const throw()
	{
		return (m_thread != NULL);
	}
	inline bool start (unsigned(__stdcall*fn)(void*),
		LPVOID param = NULL, bool suspended = false) throw()
	{
		if (!m_thread)
			m_thread = reinterpret_cast <HANDLE> (_beginthreadex (NULL, 0, fn, param,
				(suspended ? CREATE_SUSPENDED : 0), &m_id));
		return (m_thread != NULL);
	}
	inline bool is_suspended () const throw()
	{
		return m_suspended;
	}
	inline bool wait_for_stop (DWORD timeout = INFINITE) throw()
	{
		while ( m_thread )
		{
			resume ();
			MSG msg;
			while (PeekMessage (&msg, NULL, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
			DWORD reason = MsgWaitForMultipleObjects (1, &m_thread, FALSE,
				((timeout == INFINITE) ? 250 : timeout), QS_ALLINPUT);
			switch (reason)
			{
				case WAIT_OBJECT_0 + 1:
					break;

				case WAIT_TIMEOUT:
					if (timeout != INFINITE)
						return false;
					break;

				default: // WAIT_OBJECT_0, WAIT_ABANDONED_0, WAIT_FAILED
					close ();
			}
		}
		return true;
	}
	inline bool is_running () const throw()
	{
		return m_thread && (CoWaitForSingleObject (m_thread, 0) == WAIT_TIMEOUT);
	}
	inline void suspend () throw ()
	{
		if (m_thread)
			m_suspended = SuspendThread (m_thread) != (DWORD) -1;			
	}
	inline void resume () throw ()
	{
		if (m_thread) {
			DWORD ret = (DWORD) -1;
			do {
				ret = ResumeThread (m_thread);
			} while (ret > 0 && ret != (DWORD) -1);
			m_suspended = false;
		}
	}
	inline void close () throw ()
	{
		if (m_thread) {
			locker_holder h (&m_guard);
			if (m_thread) {
				CloseHandle (m_thread);
				m_thread = NULL;
			}
		}
	}
	inline int priority () const throw ()
	{
		if (m_thread)
			return GetThreadPriority (m_thread);
		else
			return THREAD_PRIORITY_ERROR_RETURN;
	}
	inline void priority (int p) throw ()
	{
		if (m_thread)
			SetThreadPriority (m_thread, p);
	}

protected:
	cs				m_guard;
	HANDLE			m_thread;
	unsigned int	m_id;
	bool			m_suspended;
};

#define THREAD_INLINE(ClassName,Fn) \
	inline bool start##Fn (bool suspended = false) throw() { \
		m_term##Fn.reset (); \
		return m_thread##Fn.start (stub##Fn, \
			reinterpret_cast<LPVOID> (this), suspended); \
	} \
	inline void stop##Fn () throw() { \
		m_term##Fn.set (); \
		m_thread##Fn.wait_for_stop (); \
	} \
	inline void suspend##Fn () throw () { \
		if (!m_term##Fn.check ()) \
			m_thread##Fn.suspend (); \
	} \
	inline void resume##Fn () throw () { \
		m_thread##Fn.resume (); \
	} \
	c4u::event m_term##Fn; \
	c4u::thread m_thread##Fn; \
	static unsigned __stdcall stub##Fn (void* param) throw() { \
		ClassName* self = reinterpret_cast <ClassName*> (param); \
		self->##Fn (); \
		return 0; \
	}

#define THREAD(ClassName,Fn) \
	void Fn (); \
	THREAD_INLINE(ClassName,Fn)

}
