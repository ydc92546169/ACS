/*******************************************************************************
* ALMA - Atacama Large Millimiter Array
* (c) European Southern Observatory, 2011
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*
* "@(#) $Id: bulkDataNTSenderFlow.cpp,v 1.19 2011/10/19 17:32:17 bjeram Exp $"
*
* who       when      what
* --------  --------  ----------------------------------------------
* bjeram  2011-04-19  created
*/

static char *rcsId="@(#) $Id: bulkDataNTSenderFlow.cpp,v 1.19 2011/10/19 17:32:17 bjeram Exp $";
static void *use_rcsId = ((void)&use_rcsId,(void *) &rcsId);

#include "bulkDataNTSenderFlow.h"
#include <iostream>
#include <AV/FlowSpec_Entry.h>  // we need it for TAO_Tokenizer ??
#include "ACS_BD_Errors.h"

using namespace AcsBulkdata;
using namespace std;
using namespace ACS_DDS_Errors;
using namespace ACS_BD_Errors;

BulkDataNTSenderFlow::BulkDataNTSenderFlow(BulkDataNTSenderStream *senderStream,
											const char* flowName,
											const SenderFlowConfiguration &sndCfg/*, cb*/) :
		senderStream_m(senderStream), flowName_m(flowName),
		ddsPublisher_m(0), ddsTopic_m(0), writerReaderListener_m(0), ddsDataWriter_m(0), frame_m(0)
{
	AUTO_TRACE(__PRETTY_FUNCTION__);
	std::string topicName;

	// should be reactor to have just one object for comunication !! DDSDataWriter or similar
	ddsPublisher_m = new BulkDataNTDDSPublisher(senderStream_m->getDDSParticipant(), sndCfg);

	topicName = senderStream_m->getName() + "#" + flowName_m;
	ddsTopic_m = ddsPublisher_m->createDDSTopic(topicName.c_str());

	writerReaderListener_m = new BulkDataNTWriterListener(topicName.c_str());
	ddsDataWriter_m= ddsPublisher_m->createDDSWriter(ddsTopic_m, writerReaderListener_m);

	//RTI probably is enough to create frame once
	frame_m = ACSBulkData::BulkDataNTFrameTypeSupport::create_data();
	if (frame_m == 0)
	{
		//TBD delete dw/topic/publisher
		DDSCreateDataProblemExImpl ex(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		ex.setDataType("ACSBulkData::BulkDataNTFrameTypeSupport");
		throw ex;
	}//if
}//BulkDataNTSenderFlow


BulkDataNTSenderFlow::~BulkDataNTSenderFlow()
{
	AUTO_TRACE(__PRETTY_FUNCTION__);
	DDS::ReturnCode_t ret;

	// no matter what happen we remove flow from the map
	senderStream_m->removeFlowFromMap(flowName_m.c_str());

	ret = ACSBulkData::BulkDataNTFrameTypeSupport::delete_data(frame_m);
	if (ret != DDS::RETCODE_OK) {
		ACS_LOG(LM_RUNTIME_CONTEXT, __PRETTY_FUNCTION__, (LM_ERROR, "BulkDataNTFrameTypeSupport::delete_data failed"));
	}//if

	// this part can go to BulkDataNTDDSPublisher, anyway we need to refactor
	DDS::DomainParticipant *participant = senderStream_m->getDDSParticipant();
	if (participant!=0)
	{
		ddsPublisher_m->destroyDDSWriter(ddsDataWriter_m);
		ddsDataWriter_m = 0;
		delete writerReaderListener_m;
		writerReaderListener_m = 0;
		ret = participant->delete_topic(ddsTopic_m);
		if (ret!=DDS::RETCODE_OK)
		{
			ACS_SHORT_LOG((LM_ERROR, "Problem deleting topic (%d)", ret));
		}
		ddsTopic_m=0;
	}
	else
	{
		ACS_LOG(LM_RUNTIME_CONTEXT, __PRETTY_FUNCTION__, (LM_ERROR, "Problem deleting data write and topic participant is NULL"));
	}
	delete ddsPublisher_m;
	ddsPublisher_m=0;
}//~BulkDataNTSenderFlow


unsigned int BulkDataNTSenderFlow::getNumberOfReceivers()
{
	AUTO_TRACE(__PRETTY_FUNCTION__);
	DDS::ReliableReaderActivityChangedStatus status;

	ddsDataWriter_m->get_reliable_reader_activity_changed_status(status);
	return status.active_count;
}//getNumberOfReceivers

void BulkDataNTSenderFlow::startSend(ACE_Message_Block *param)
{
 startSend((unsigned char*)(param->rd_ptr()), param->length());
}

void BulkDataNTSenderFlow::startSend(const unsigned char *param, size_t len)
{
	writeFrame(ACSBulkData::BD_PARAM, param, len);
}//startSend


void BulkDataNTSenderFlow::sendData(ACE_Message_Block *buffer)
{
	 sendData((unsigned char*)(buffer->rd_ptr()), buffer->length());
}//sendData

void BulkDataNTSenderFlow::sendData(const unsigned char *buffer, size_t len)
{
	unsigned int sizeOfFrame = ACSBulkData::FRAME_MAX_LEN;  //TBD: tmp should be configurable

	unsigned int numOfFrames = len / sizeOfFrame; // how many frames of size sizeOfDataChunk do we have to send
	unsigned int restFrameSize = len % sizeOfFrame; // what rests ?

	frame_m->dataType = ACSBulkData::BD_DATA;  //we are going to send data
	frame_m->data.length(sizeOfFrame); // frame.data.resize(sizeOfFrame);; // do we actually need resize ?

	ACS_SHORT_LOG((LM_DEBUG, "Going to send: %d Bytes = %d*%d(=%d) + %d to flow: %s",
			len, numOfFrames, sizeOfFrame, numOfFrames*sizeOfFrame, restFrameSize, flowName_m.c_str()));

//	start_time = ACE_OS::gettimeofday();

	unsigned int numOfIter = (restFrameSize>0) ? numOfFrames+1 : numOfFrames;

	for(unsigned int i=0; i<numOfIter; i++)
	{
		if (i==(numOfIter-1) && restFrameSize>0)
		{
			// last frame
			 writeFrame(ACSBulkData::BD_DATA, (buffer+(i*sizeOfFrame)), restFrameSize, numOfIter-1-i);
		}else
		{
			 writeFrame(ACSBulkData::BD_DATA, (buffer+(i*sizeOfFrame)), sizeOfFrame, numOfIter-1-i);
		}
	}//for
 // at this point we have sent all frames, we coudl wait fro ACK, but it is done in writeFrame
}//sendData

void BulkDataNTSenderFlow::stopSend()
{
	writeFrame(ACSBulkData::BD_STOP);
}//stopSend

void BulkDataNTSenderFlow::writeFrame(ACSBulkData::DataType dataType,  const unsigned char *param, size_t len, unsigned int restFrameCount)
{
	DDS::ReturnCode_t ret;
	DDS::ReliableWriterCacheChangedStatus status; //RTI
	// should we wait for all ACKs? timeout should be configurable
	DDS::Duration_t ack_timeout_delay = {1, 0};//1s

	if (len>ACSBulkData::FRAME_MAX_LEN){
		FrameTooLongExImpl ftlEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		ftlEx.setFrameLength(len);
		ftlEx.setMaxFrameLength(ACSBulkData::FRAME_MAX_LEN);
		throw ftlEx;
	}//if

	//frame
	frame_m->dataType = dataType;
	frame_m->data./*resize*/length(len);
	frame_m->restDataLength = restFrameCount; //we need it just in soem cases, but we can always set to 0
	if (param!=0 && len!=0)
		frame_m->data.from_array(param, len); //frame.data.assign(len, *param);   //1st parameter  is of type const unsigned char !!

	ret = ddsDataWriter_m->write(*frame_m, DDS::HANDLE_NIL);
	if( ret != DDS::RETCODE_OK)
	{
		ddsDataWriter_m->get_reliable_writer_cache_changed_status(status); //RTI
		if (ret==DDS::RETCODE_TIMEOUT)
		{
			SendFrameTimeoutExImpl toEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			toEx.setSenderName(senderStream_m->getName().c_str()); toEx.setFlowName(flowName_m.c_str());
			toEx.setTimeout(0.0); //TBD: put value from QoS
			//toEx.setFrameCount(i); toEx.setTotalFrameCount(numOfIter);
			throw toEx;
			//ACS_LOG(LM_RUNTIME_CONTEXT, __PRETTY_FUNCTION__, (LM_ERROR, "Timeout while sending data @ %d", i));
		}else
		{
			SendFrameErrorExImpl sfEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			sfEx.setSenderName(senderStream_m->getName().c_str()); sfEx.setFlowName(flowName_m.c_str());
			//sfEx.setFrameCount(i); sfEx.setTotalFrameCount(numOfIter);
			sfEx.setRetCode(ret);
			throw sfEx;
			//ACS_LOG(LM_RUNTIME_CONTEXT, __PRETTY_FUNCTION__, (LM_ERROR, "Failed to send @ %d (%d)", i, ret));
		}//if-else

		ddsDataWriter_m->get_reliable_writer_cache_changed_status(status); //RTI
		ACS_SHORT_LOG((LM_DEBUG, "unacknowledged_sample_count (%d) for flow: %s", status.unacknowledged_sample_count, flowName_m.c_str())); //RTI
		// RTI			cout << "\t\t Int unacknowledged_sample_count_peak: " << status.unacknowledged_sample_count_peak << endl;
		ret = ddsDataWriter_m->wait_for_acknowledgments(ack_timeout_delay);
		if( ret != DDS::RETCODE_OK)
		{
			FrameAckTimeoutExImpl ackToEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			ackToEx.setSenderName(senderStream_m->getName().c_str()); ackToEx.setFlowName(flowName_m.c_str());
			ackToEx.setTimeout(ack_timeout_delay.sec + ack_timeout_delay.nanosec/1000000.0);
			//ackToEx.setFrameCount(i); ackToEx.setTotalFrameCount(numOfIter);
			ackToEx.log(LM_WARNING); //TBD should be an error ?
			//ACS_LOG(LM_RUNTIME_CONTEXT, __PRETTY_FUNCTION__, (LM_WARNING, "wait_for_acknowledgments at frame level time-outed"));
		}//if

		ddsDataWriter_m->get_reliable_writer_cache_changed_status(status); //RTI
		ACS_SHORT_LOG((LM_DEBUG, "unacknowledged_sample_count after waiting: (%d)", status.unacknowledged_sample_count)); //RTI
		//cout << "\t\t Int1 unacknowledged_sample_count_peak: " << status.unacknowledged_sample_count_peak << endl;
	}//if (ret != DDS::RETCODE_OK)

	if (restFrameCount==0) //we wait for ACKs just if it was the only frame, or the last frame
	{
		ddsDataWriter_m->get_reliable_writer_cache_changed_status(status); //RTI
		ACS_SHORT_LOG((LM_DEBUG, "unacknowledged_sample_count (%d) before waiting: %d", dataType, status.unacknowledged_sample_count)); //RTI
		ret = ddsDataWriter_m->wait_for_acknowledgments(ack_timeout_delay);
		if( ret != DDS::RETCODE_OK)
		{
			FrameAckTimeoutExImpl ackToEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			ackToEx.setSenderName(senderStream_m->getName().c_str()); ackToEx.setFlowName(flowName_m.c_str());
			ackToEx.setTimeout(ack_timeout_delay.sec + ack_timeout_delay.nanosec/1000000.0);
			//ackToEx.setFrameCount(i); ackToEx.setTotalFrameCount(numOfIter);
			ackToEx.log(LM_WARNING); //TBD should be an error ?
			//ACS_LOG(LM_RUNTIME_CONTEXT, __PRETTY_FUNCTION__, (LM_WARNING, "wait_for_acknowledgments at frame level time-outed"));
		}//if
	}//if (restFrameCount==0)
}//writeFrame

/*___oOo___*/
