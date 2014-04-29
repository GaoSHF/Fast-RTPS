/*************************************************************************
 * Copyright (c) 2013 eProsima. All rights reserved.
 *
 * This copy of FastCdr is licensed to you under the terms described in the
 * EPROSIMARTPS_LIBRARY_LICENSE file included in this distribution.
 *
 *************************************************************************/

/*
 * ThreadListen.cpp
 *
 *  Created on: Feb 25, 2014
 *      Author: Gonzalo Rodriguez Canosa
 *      email:  gonzalorodriguez@eprosima.com
 *      		grcanosa@gmail.com
 */

#include "eprosimartps/threadtype/ThreadListen.h"
#include "eprosimartps/CDRMessage.h"
#include "eprosimartps/writer/RTPSWriter.h"
#include "eprosimartps/reader/RTPSReader.h"
#include "eprosimartps/Participant.h"

using boost::asio::ip::udp;


namespace eprosima {
namespace rtps {

ThreadListen::ThreadListen() :
												m_participant_ptr(NULL),mp_thread(NULL),
												m_listen_socket(m_io_service),
												m_first(true)
{
	m_MessageReceiver.mp_threadListen = this;
	m_isMulticast = false;
}

ThreadListen::~ThreadListen()
{
	pWarning( "Removing listening thread " << mp_thread->get_id() << std::endl);

	//	cout << "shutdown" << endl;
	//	m_listen_socket.shutdown(boost::asio::ip::udp::socket::shutdown_receive);
	//	cout << "done"<<endl;
	m_listen_socket.close();
	m_io_service.stop();
	pInfo("Joining with thread"<<endl);
	mp_thread->join();


	delete(mp_thread);

}

void ThreadListen::run_io_service()
{

	pInfo ( BLUE << "Thread: " << mp_thread->get_id() << " listening in IP: " <<m_listen_socket.local_endpoint() << DEF << endl) ;
	m_participant_ptr->m_ThreadSemaphore->post();
	m_first = false;

	this->m_io_service.run();
}

bool ThreadListen::init_thread(){
	if(!m_locList.empty())
	{
		m_first = true;
		m_listen_socket.open(boost::asio::ip::udp::v4());
		boost::asio::ip::address address = boost::asio::ip::address::from_string(m_locList.begin()->to_IP4_string());
		udp::endpoint listen_endpoint(boost::asio::ip::udp::v4(),m_locList.begin()->port);
		if(m_isMulticast)
		{
			m_listen_socket.set_option( boost::asio::ip::udp::socket::reuse_address( true ) );
			m_listen_socket.set_option( boost::asio::ip::multicast::enable_loopback( false ) );
			listen_endpoint = udp::endpoint(address,m_locList.begin()->port);
		}

		try{

			m_listen_socket.bind(listen_endpoint);
			if(m_isMulticast)
			{

				m_listen_socket.set_option( boost::asio::ip::multicast::join_group( address ) );

			}
		}
		catch (boost::system::system_error const& e)
		{
			pError(e.what() << endl);
			return false;
		}
		//CDRMessage_t msg;
		CDRMessage::initCDRMsg(&m_MessageReceiver.m_rec_msg);
		m_listen_socket.async_receive_from(
				boost::asio::buffer((void*)m_MessageReceiver.m_rec_msg.buffer, m_MessageReceiver.m_rec_msg.max_size),
				m_sender_endpoint,
				boost::bind(&ThreadListen::newCDRMessage, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
		mp_thread = new boost::thread(&ThreadListen::run_io_service,this);
		return true;
	}
	return false;
}


void ThreadListen::newCDRMessage(const boost::system::error_code& err, std::size_t msg_size)
{
	if(err == boost::system::errc::success)
	{
		m_MessageReceiver.m_rec_msg.length = msg_size;

		if(m_MessageReceiver.m_rec_msg.length == 0)
		{
			return;
		}
		pInfo (BLUE << "Message received of length: " << m_MessageReceiver.m_rec_msg.length << " from endpoint: " << m_sender_endpoint << DEF << endl);

		//Get address into Locator
		m_send_locator.port = m_sender_endpoint.port();
		LOCATOR_ADDRESS_INVALID(m_send_locator.address);
		for(int i=0;i<4;i++)
		{
			m_send_locator.address[i+12] = m_sender_endpoint.address().to_v4().to_bytes()[i];
		}
		try
		{
			m_MessageReceiver.processCDRMsg(m_participant_ptr->m_guid.guidPrefix,&m_send_locator,&m_MessageReceiver.m_rec_msg);
			pInfo (BLUE<<"Message processed " <<DEF<< endl);
		}
		catch(int e)
		{
			pError( "Error processing message of type: " << e << std::endl);

		}
		//CDRMessage_t msg;
		pInfo(BLUE<< "Socket async receive put again to listen "<<DEF<< endl);
		CDRMessage::initCDRMsg(&m_MessageReceiver.m_rec_msg);
		m_listen_socket.async_receive_from(
				boost::asio::buffer((void*)m_MessageReceiver.m_rec_msg.buffer, m_MessageReceiver.m_rec_msg.max_size),
				m_sender_endpoint,
				boost::bind(&ThreadListen::newCDRMessage, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	}
	else if(err == boost::asio::error::operation_aborted)
	{
		pInfo("Operation in listening socket aborted..."<<endl);
		return;
	}
	else
	{
		//CDRMessage_t msg;
		pInfo(BLUE<< "Socket async receive put again to listen "<<DEF<< endl);
		CDRMessage::initCDRMsg(&m_MessageReceiver.m_rec_msg);
		m_listen_socket.async_receive_from(
				boost::asio::buffer((void*)m_MessageReceiver.m_rec_msg.buffer, m_MessageReceiver.m_rec_msg.max_size),
				m_sender_endpoint,
				boost::bind(&ThreadListen::newCDRMessage, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	}
}



} /* namespace rtps */
} /* namespace eprosima */

