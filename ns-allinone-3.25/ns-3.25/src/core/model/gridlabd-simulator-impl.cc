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

#include "simulator.h"
#include "gridlabd-simulator-impl.h"
#include "scheduler.h"
#include "event-impl.h"

#include "ptr.h"
#include "pointer.h"
#include "assert.h"
#include "log.h"

#include <string>
#include <cmath>

// Note:  Logging in this file is largely avoided due to the
// number of calls that are made to these functions and the possibility
// of causing recursions leading to stack overflow

NS_LOG_COMPONENT_DEFINE("GridlabdSimulatorImpl");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(GridlabdSimulatorImpl);

TypeId GridlabdSimulatorImpl::GetTypeId(void) {
	static TypeId tid = TypeId("ns3::GridlabdSimulatorImpl").SetParent<
			SimulatorImpl>().AddConstructor<GridlabdSimulatorImpl>();
	return tid;
}

GridlabdSimulatorImpl::GridlabdSimulatorImpl() {
	NS_LOG_FUNCTION (this);
	m_stop = false;
	// uids are allocated from 4.
	// uid 0 is "invalid" events
	// uid 1 is "now" events
	// uid 2 is "destroy" events
	m_uid = 4;
	// before ::Run is entered, the m_currentUid will be zero
	m_currentUid = 0;
	m_currentTs = 0;
	m_timeGrant = 0;
	m_currentContext = 0xffffffff;
	m_unscheduledEvents = 0;
	m_eventsWithContextEmpty = true;
	m_main = SystemThread::Self();
}

GridlabdSimulatorImpl::~GridlabdSimulatorImpl() {
	NS_LOG_FUNCTION (this);
}

void GridlabdSimulatorImpl::DoDispose(void) {
	NS_LOG_FUNCTION (this);
	while (!m_events->IsEmpty()) {
		Scheduler::Event next = m_events->RemoveNext();
		next.impl->Unref();
	}
	m_events = 0;
	SimulatorImpl::DoDispose();
}
void GridlabdSimulatorImpl::Destroy() {
	NS_LOG_FUNCTION (this);
	while (!m_destroyEvents.empty()) {
		Ptr<EventImpl> ev = m_destroyEvents.front().PeekEventImpl();
		m_destroyEvents.pop_front();
		NS_LOG_LOGIC ("handle destroy " << ev);
		if (!ev->IsCancelled()) {
			ev->Invoke();
		}
	}
}

void GridlabdSimulatorImpl::SetScheduler(ObjectFactory schedulerFactory) {
	NS_LOG_FUNCTION (this << schedulerFactory);
	Ptr<Scheduler> scheduler = schedulerFactory.Create<Scheduler>();

	if (m_events != 0) {
		while (!m_events->IsEmpty()) {
			Scheduler::Event next = m_events->RemoveNext();
			scheduler->Insert(next);
		}
	}
	m_events = scheduler;
}

// System ID for non-distributed simulation is always zero
uint32_t GridlabdSimulatorImpl::GetSystemId(void) const {
	return 0;
}

void GridlabdSimulatorImpl::ProcessOneEvent(void) {
	Scheduler::Event next = m_events->RemoveNext();

	NS_ASSERT(next.key.m_ts >= m_currentTs);
	m_unscheduledEvents--;

	NS_LOG_LOGIC ("handle " << next.key.m_ts);
	m_currentTs = next.key.m_ts;
	m_currentContext = next.key.m_context;
	m_currentUid = next.key.m_uid;
	next.impl->Invoke();
	next.impl->Unref();

	ProcessEventsWithContext();
}

bool GridlabdSimulatorImpl::IsFinished(void) const {
	return m_events->IsEmpty() || m_stop;
}

void GridlabdSimulatorImpl::ProcessEventsWithContext(void) {
	if (m_eventsWithContextEmpty) {
		return;
	}

	// swap queues
	EventsWithContext eventsWithContext;
	{
		CriticalSection cs(m_eventsWithContextMutex);
		m_eventsWithContext.swap(eventsWithContext);
		m_eventsWithContextEmpty = true;
	}
	while (!eventsWithContext.empty()) {
		EventWithContext event = eventsWithContext.front();
		eventsWithContext.pop_front();
		Scheduler::Event ev;
		ev.impl = event.event;
		ev.key.m_ts = m_currentTs + event.timestamp;
		ev.key.m_context = event.context;
		ev.key.m_uid = m_uid;
		m_uid++;
		m_unscheduledEvents++;
		m_events->Insert(ev);
	}
}

void GridlabdSimulatorImpl::InitializeRun() {
	NS_LOG_FUNCTION (this);
	// Set the current threadId as the main threadId
	m_main = SystemThread::Self();
	ProcessEventsWithContext();
	m_stop = false;

	//Devendra Shelar print statements before each event
	printf("gridlabd-simulator-impl.cc : Run Started\n");
}

void GridlabdSimulatorImpl::ExecuteEventsTillTimeGrant() {
	while (!m_events->IsEmpty() && !m_stop && m_timeGrant >= m_currentTs) {

		/*printf(
				"gridlabd-simulator-impl.cc : Scheduling one event at a time %llu timeGrant %llu\n",
				m_currentTs, m_timeGrant);
*/
		ProcessOneEvent();
	}

}

void GridlabdSimulatorImpl::Run(void) {
	NS_LOG_FUNCTION (this);
	// Set the current threadId as the main threadId
	m_main = SystemThread::Self();
	ProcessEventsWithContext();
	m_stop = false;

	//Devendra Shelar print statements before each event
	printf("gridlabd-simulator-impl.cc : Run Started\n");

	/*NodeContainer n = NodeContainer::GetGlobal();
	 uint32_t i = 0;
	 for ( ; i < n.GetN() ; i++){
	 Ptr<Node> n1 = n.Get(i);
	 printf("GridlabdSimulatorImpl : node id %d\n", n1->GetId());
	 }


	 uint64_t timeStepIteration = 0;
	 */
	//char userInput[20];
	while (!m_events->IsEmpty() && !m_stop) {
		//int sex = (int) m_currentTs;
//		printf(
//				"gridlabd-simulator-impl.cc : Scheduling one event at a time %llu timeGrant %llu\n",
//				m_currentTs, m_timeGrant);

		while (m_timeGrant <= m_currentTs && !m_stop) {
//			printf("gridlabd-simulator-impl.cc : Press enter\n");

//			 scanf("%s", userInput);
			 //m_timeGrant = m_timeGrant + 1000000000;
//			printf(
//					"gridlabd-simulator-impl.cc : Scheduling one event at a time %llu timeGrant %llu\n",
//					m_currentTs, m_timeGrant);
		}

		ProcessOneEvent();
	}

	// If the simulator stopped naturally by lack of events, make a
	// consistency test to check that we didn't lose any events along the way.
	NS_ASSERT(!m_events->IsEmpty() || m_unscheduledEvents == 0);
}

void GridlabdSimulatorImpl::Stop(void) {
	NS_LOG_FUNCTION (this);
	m_stop = true;
}

void GridlabdSimulatorImpl::Stop(Time const &time) {
	NS_LOG_FUNCTION (this << time.GetTimeStep ());
	Simulator::Schedule(time, &Simulator::Stop);
}

//
// Schedule an event for a _relative_ time in the future.
//
EventId GridlabdSimulatorImpl::Schedule(Time const &time, EventImpl *event) {
	NS_LOG_FUNCTION (this << time.GetTimeStep () << event);
	NS_ASSERT_MSG(SystemThread::Equals(m_main),
			"Simulator::Schedule Thread-unsafe invocation!");

	Time tAbsolute = time + TimeStep(m_currentTs);

	NS_ASSERT(tAbsolute.IsPositive());
	NS_ASSERT(tAbsolute >= TimeStep(m_currentTs));
	Scheduler::Event ev;
	ev.impl = event;
	ev.key.m_ts = (uint64_t) tAbsolute.GetTimeStep();
	ev.key.m_context = GetContext();
	ev.key.m_uid = m_uid;
	m_uid++;
	m_unscheduledEvents++;
	m_events->Insert(ev);
	return EventId(event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

void GridlabdSimulatorImpl::ScheduleWithContext(uint32_t context,
		Time const &time, EventImpl *event) {
	NS_LOG_FUNCTION (this << context << time.GetTimeStep () << event);

	if (SystemThread::Equals(m_main)) {
		Time tAbsolute = time + TimeStep(m_currentTs);
		Scheduler::Event ev;
		ev.impl = event;
		ev.key.m_ts = (uint64_t) tAbsolute.GetTimeStep();
		ev.key.m_context = context;
		ev.key.m_uid = m_uid;
		m_uid++;
		m_unscheduledEvents++;
		m_events->Insert(ev);
	} else {
		EventWithContext ev;
		ev.context = context;
		ev.timestamp = time.GetTimeStep();
		ev.event = event;
		{
			CriticalSection cs(m_eventsWithContextMutex);
			m_eventsWithContext.push_back(ev);
			m_eventsWithContextEmpty = false;
		}
	}
}

EventId GridlabdSimulatorImpl::ScheduleNow(EventImpl *event) {
	NS_ASSERT_MSG(SystemThread::Equals(m_main),
			"Simulator::ScheduleNow Thread-unsafe invocation!");

	Scheduler::Event ev;
	ev.impl = event;
	ev.key.m_ts = m_currentTs;
	ev.key.m_context = GetContext();
	ev.key.m_uid = m_uid;
	m_uid++;
	m_unscheduledEvents++;
	m_events->Insert(ev);
	return EventId(event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

EventId GridlabdSimulatorImpl::ScheduleDestroy(EventImpl *event) {
	NS_ASSERT_MSG(SystemThread::Equals(m_main),
			"Simulator::ScheduleDestroy Thread-unsafe invocation!");

	EventId id(Ptr<EventImpl>(event, false), m_currentTs, 0xffffffff, 2);
	m_destroyEvents.push_back(id);
	m_uid++;
	return id;
}

Time GridlabdSimulatorImpl::Now(void) const {
	// Do not add function logging here, to avoid stack overflow
	return TimeStep(m_currentTs);
}

Time GridlabdSimulatorImpl::GetDelayLeft(const EventId &id) const {
	if (IsExpired(id)) {
		return TimeStep(0);
	} else {
		return TimeStep(id.GetTs() - m_currentTs);
	}
}

void GridlabdSimulatorImpl::Remove(const EventId &id) {
	if (id.GetUid() == 2) {
		// destroy events.
		for (DestroyEvents::iterator i = m_destroyEvents.begin();
				i != m_destroyEvents.end(); i++) {
			if (*i == id) {
				m_destroyEvents.erase(i);
				break;
			}
		}
		return;
	}
	if (IsExpired(id)) {
		return;
	}
	Scheduler::Event event;
	event.impl = id.PeekEventImpl();
	event.key.m_ts = id.GetTs();
	event.key.m_context = id.GetContext();
	event.key.m_uid = id.GetUid();
	m_events->Remove(event);
	event.impl->Cancel();
	// whenever we remove an event from the event list, we have to unref it.
	event.impl->Unref();

	m_unscheduledEvents--;
}

void GridlabdSimulatorImpl::Cancel(const EventId &id) {
	if (!IsExpired(id)) {
		id.PeekEventImpl()->Cancel();
	}
}

bool GridlabdSimulatorImpl::IsExpired(const EventId &ev) const {
	if (ev.GetUid() == 2) {
		if (ev.PeekEventImpl() == 0 || ev.PeekEventImpl()->IsCancelled()) {
			return true;
		}
		// destroy events.
		for (DestroyEvents::const_iterator i = m_destroyEvents.begin();
				i != m_destroyEvents.end(); i++) {
			if (*i == ev) {
				return false;
			}
		}
		return true;
	}
	if (ev.PeekEventImpl() == 0 || ev.GetTs() < m_currentTs
			|| (ev.GetTs() == m_currentTs && ev.GetUid() <= m_currentUid)
			|| ev.PeekEventImpl()->IsCancelled()) {
		return true;
	} else {
		return false;
	}
}

Time GridlabdSimulatorImpl::GetMaximumSimulationTime(void) const {
	/// \todo I am fairly certain other compilers use other non-standard
	/// post-fixes to indicate 64 bit constants.
	return TimeStep(0x7fffffffffffffffLL);
}

uint32_t GridlabdSimulatorImpl::GetContext(void) const {
	return m_currentContext;
}

bool GridlabdSimulatorImpl::IsWaiting() {
	return m_wait;
}

void GridlabdSimulatorImpl::SetWaiting(bool wait) {
	m_wait = wait;
}

Time GridlabdSimulatorImpl::GetTimeGrant() {
	printf("GridlabdSimulatorImpl::Get Time Grant to be %llu\n", m_timeGrant);
	return TimeStep(m_timeGrant);
}

void GridlabdSimulatorImpl::SetTimeGrant(Time const &time) {
	m_timeGrant = (uint64_t) time.GetTimeStep();
	printf("GridlabdSimulatorImpl::Set Time Grant to %llu\n", m_timeGrant);
}

bool GridlabdSimulatorImpl::IsSimulationActive() {
	return !m_events->IsEmpty();
}

} // namespace ns3