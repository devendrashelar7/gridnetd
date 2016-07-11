/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#ifndef GRIDLABD_SIMULATOR_IMPL_H
#define GRIDLABD_SIMULATOR_IMPL_H

#include "simulator-impl.h"
#include "scheduler.h"
#include "event-impl.h"
#include "system-thread.h"
#include "ns3/system-mutex.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/names.h"

#include "ptr.h"

#include <list>

namespace ns3 {

/**
 * \ingroup simulator
 */
class GridlabdSimulatorImpl: public SimulatorImpl {
public:
	static TypeId GetTypeId(void);

	GridlabdSimulatorImpl();
	~GridlabdSimulatorImpl();

	virtual void Destroy();
	virtual bool IsFinished(void) const;
	virtual void Stop(void);
	virtual void Stop(Time const &time);
	virtual EventId Schedule(Time const &time, EventImpl *event);
	virtual void ScheduleWithContext(uint32_t context, Time const &time,
			EventImpl *event);
	virtual EventId ScheduleNow(EventImpl *event);
	virtual EventId ScheduleDestroy(EventImpl *event);
	virtual void Remove(const EventId &ev);
	virtual void Cancel(const EventId &ev);
	virtual bool IsExpired(const EventId &ev) const;
	virtual void Run(void);
	virtual Time Now(void) const;
	virtual Time GetDelayLeft(const EventId &id) const;
	virtual Time GetMaximumSimulationTime(void) const;
	virtual void SetScheduler(ObjectFactory schedulerFactory);
	virtual uint32_t GetSystemId(void) const;
	virtual uint32_t GetContext(void) const;
	virtual bool IsWaiting();
	virtual void SetWaiting(bool wait);
	virtual Time GetTimeGrant();
	virtual void SetTimeGrant(Time const &time);
	virtual bool IsSimulationActive();

	void InitializeRun();
	void ExecuteEventsTillTimeGrant();
	void ProcessOneEvent(void);
	void ProcessEventsWithContext(void);

private:
	virtual void DoDispose(void);

	struct EventWithContext {
		uint32_t context;
		uint64_t timestamp;
		EventImpl *event;
	};
	typedef std::list<struct EventWithContext> EventsWithContext;
	EventsWithContext m_eventsWithContext;
	bool m_eventsWithContextEmpty;
	SystemMutex m_eventsWithContextMutex;

	typedef std::list<EventId> DestroyEvents;
	DestroyEvents m_destroyEvents;
	bool m_stop;
	bool m_wait;
	Ptr<Scheduler> m_events;

	uint32_t m_uid;
	uint32_t m_currentUid;
	uint64_t m_currentTs;
	uint32_t m_currentContext;
	uint64_t m_timeGrant;
	// number of events that have been inserted but not yet scheduled,
	// not counting the "destroy" events; this is used for validation
	int m_unscheduledEvents;

	SystemThread::ThreadId m_main;
};

} // namespace ns3

#endif /* GRIDLABD_SIMULATOR_IMPL_H */
