#ifndef CONSUMER_H
#define CONSUMER_H

/* @(#) $Id: acsncConsumer.h,v 1.59 2006/03/08 17:50:44 dfugate Exp $
*
*    Consumer Abstract base class for notification channel push structured event
*    consumers.
*    ALMA - Atacama Large Millimiter Array
*
*    (c) Associated Universities Inc., 2002   
*    (c) European Southern Observatory, 2002
*    Copyright by ESO (in the framework of the ALMA collaboration)
*    and Cosylab 2002, All rights reserved
*
*    This library is free software; you can redistribute it and/or
*    modify it under the terms of the GNU Lesser General Public
*    License as published by the Free Software Foundation; either
*    version 2.1 of the License, or (at your option) any later version.
*
*    This library is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    Lesser General Public License for more details.
*
*    You should have received a copy of the GNU Lesser General Public
*    License along with this library; if not, write to the Free Software
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

/** @file acsncConsumer.h
 *  Header file for Consumer.
 */

#include "acsncHelper.h"
#include <acstimeProfiler.h>
#include "acsncCDBProperties.h"

#include <list>
NAMESPACE_BEGIN(nc);

/** Consumer implements a structured event push consumer interface from the
 *  CORBA notification service (NC). This class is an abstract base class due to
 *  the pure virtual function: push_structured_event(). Derived classes must
 *  implement this function plus any others which facilitate the consuming of
 *  data, for example, a callback function.
 *
 *  TODO:
 *  - add a mutex for the polling thread???
 *  - check for both local and remote memory leaks
 */
class Consumer :
    protected Helper,
    public POA_CosNotifyComm::StructuredPushConsumer,
    protected virtual PortableServer::RefCountServantBase
{
  public:
    /** 
     *  Constructor to be used within components.
     *  Consumer will use the Container to get a reference to the Naming
     *  Service.  If a valid reference to the container cannot be obtained, Consumer 
     *  will default to creating it's own ORB (assuming Consumer is run on the same
     *  host as the Naming Service).
     *  @param channelName name of the channel to connect to.
     */
    Consumer(const char* channelName);

    /** 
     *  Constructor to be used within SimpleClient's.
     *  This constructor is provided for API users who create their own ORB that has 
     *  a reference to the Naming Service.
     *  @param channelName name of the channel to connect too.
     *  @param orb_mp ORB that <b>has</b> a valid reference to the Naming Service.
     */
    Consumer(const char* channelName, 
	     CORBA::ORB_ptr orb_mp);
    
    /** 
     *  Optional constructor - used outside of ACS.
     *  This constructor is very resource intensive (it spawns it's own ORB) and should 
     *  only to be utilized when there is a reason not to use Manager to get at the 
     *  Naming Service.  If argc==0, default parameters (i.e., environment variables) 
     *  specify how to get to the Naming Service. Otherwise, it is assumed argv has
     *  a valid corbaloc to the Naming Service.
     *  @param channelName name of the channel to connect to.
     *  @param argc Number of ORB parameters in argv or 0
     *  @param argv ORB params.  Typically something like:<br>
     *	orbArg[0] = ""<br>.
     *  orbArg[1] = "-ORBInitRef NameService=corbaloc::host:xxxx/NameService"<br>
     *  orbArg[2] = "-ORBDottedDecimalAddresses=1"<br>
     *  @htmlonly
        Sample Usage:<br>
        <ul>
          <li><b><i>Consumer("fridge",0,(char **)0);</b></i> - Generates ORB arguments on the fly from environment variables. 
          <li><b><i>Consumer("fridge",argc,argv);</b></i> - Uses passed ORB arguments.
        </ul>
        @endhtmlonly
     */
    Consumer(const char* channelName, 
	     int argc, 
	     char *argv[]);

    /**
     *  Disconnect from the channel. Call this instead of deleting the object.
     *  Once disconnect has been invoked, developers should treat the object
     *  as if it has been deleted.
     *
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    virtual void 
    disconnect();
    
    /** 
     *  This pure virtual function <b>must</b> be overridden by the child class
     *  to receive data from the channel. <b>Do
     *  not call it on your own!</b>
     *  @param publishedEvent The real CORBA event (defined via an IDL definition).
     *         This structure has little to do with so-called ICD events.
     *
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    virtual void 
    push_structured_event(const CosNotification::StructuredEvent &publishedEvent)
	throw (CORBA::SystemException, CosEventComm::Disconnected) = 0;
    
    /** 
     *  Supplier calls this when it has events to add or remove from the
     *  notification channel. Note that this method contains only a bare-bones
     *  implementation.<b>Do not call it on your own!</b>
     *  @param added Events the publisher will start publishing to the NC.
     *  @param removed Events the publisher will stop publishing to the NC.
     *
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    virtual void 
    offer_change(const CosNotification::EventTypeSeq &added,
		 const CosNotification::EventTypeSeq &removed)
	throw (CORBA::SystemException, CosNotifyComm::InvalidEventType);
    
    /**
     *  The Supplier calls this when it disconnects from the channel. <b>Do
     *  not call it on your own!</b>
     *
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    virtual void 
    disconnect_structured_push_consumer()
	throw (CORBA::SystemException);

    /** 
     *  After this method is invoked, the developer has little control over when 
     *  events are received (i.e., push_structured_event() is invoked).
     *
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    void
    consumerReady()
	throw (CORBAProblemEx);

    /** 
     *  A suspended (see suspend()) connection can be resumed.
     *
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    void
    resume()
	throw (CORBAProblemEx);
    
    /** 
     *  After this method is invoked, the connection is suspended. All events
     *  published on this channel will not be processed by this Consumer instance.
     *
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    void
    suspend()
	throw (CORBAProblemEx);

    /** 
     *  Subscribe to more events.
     *
     *  The template parameter T is the ALMA event type to subscribe
     *  to.
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    template <class T> void
    addSubscription()
	throw (CORBAProblemEx)
	{
	    //Create a tempory any
	    CORBA::Any any;
	    //Create a temporary instance of the ALMA event.
	    T junk;
	    
	    //Store the ALMA event in the any to extract the name
	    //Probably a nicer way of doing this but it's the best that
	    //I can come up with at the moment.
	    any <<= junk;

	    //Use the version of addSubscription which really subscribes
	    //to events from the channel
	    addSubscription(any.type()->name());
	}

    /** 
     *  Unsubscribe from events.
     *
     *  The template parameter T is the type of structured events to unsubscribe from.
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    template <class T> void
    removeSubscription()
	throw (CORBAProblemEx)
	{
	    //Create a tempory any
	    CORBA::Any any;
	    //Create a temporary instance of the ALMA event.
	    T junk;
	    
	    //Store the ALMA event in the any to extract the name
	    //Probably a nicer way of doing this but it's the best that
	    //I can come up with at the moment.
	    any <<= junk;
	    
	    //Use the version of removeSubscription which really unsubscribes
	    //from an event type.
	    removeSubscription(any.type()->name());
	}
    
    /** 
     * Filter out events meeting certain requirements. Because ALMA has 
     * chosen to define ICD events solely in terms of IDL structs, this 
     * method has little use. For interested
     * developers with free time on their hands, please see OMG's 
     * documentation on the Extended Trader Constraint language to see what 
     * filters look like used with simple CORBA types.
     *
     * @param type_name Type of structured events to filter.
     * @param filter Extended trader constraint language filter string.
     * @return ID of the filter object (used to remove the filter later).
     * @htmlonly
       <br><hr>
       @endhtmlonly
     */
    int
    addFilter(const char* type_name,
	      const char* filter)
	throw (CORBAProblemEx);
    
    /** 
     *  Remove a filter. See comments on the addFilter() method.
     *
     *  @param filter_id ID of the filter object created by the addFilter() method. 
     *  @return True on success, false otherwise.
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    bool
    removeFilter(int filter_id)
	throw (CORBAProblemEx);
    
  protected:
    /**
     * Utility method.
     * Initialization method where code common to all constructors is kept.
     * @param orb ORB which has a reference to the Naming Service.
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    void
    init()
	throw (CORBAProblemEx);

    /**
     *  Destructor is protected.
     */
    virtual ~Consumer(){}
    ///////////////////////////////////////////////////////////////////////////////////////
    /**
     * Utility method.
     * Creates a proxy supplier and consumer admin. Next connect this consumer to the
     * proxy supplier.
     * @return void
     * @htmlonly
       <br><hr>
       @endhtmlonly
     */
    void 
    createConsumer()
	throw (CORBAProblemEx);

    ///////////////////////////////////////////////////////////////////////////////////////
    /** 
     *  The consumer admin object used by consumers to get a reference to the
     *  structured supplier proxy.
     */
    CosNotifyChannelAdmin::ConsumerAdmin_var consumerAdmin_m;

    /**
     * The proxy supplier that we are connected to. This invokes methods defined in IDL
     * stub class such as push_structured_event().
     */
    CosNotifyChannelAdmin::StructuredProxyPushSupplier_var proxySupplier_m;

    /**
     * The number of events received so far. It is up do the developer to update this 
     * variable.
     */
    unsigned long long numEvents_m;
    
    /**
     * Handle destruction using an _var reference to the underlying CORBA object.
     */
    CosNotifyComm::StructuredPushConsumer_var reference_m;

    /**
     * This method returns a constant character pointer to the type of filter constraint language
     * to be used for filtering events which is normally equivalent to acsnc::FILTER_LANGUAGE_NAME.
     * Override to change this behavior.
     * @return pointer to a constant string.
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    virtual const char* 
    getFilterLanguage();

    /**
     * Maps events to the total amount of time they have to be processed.
     */
    CDBProperties::EventHandlerTimeoutMap handlerTimeoutMap_m;

    /**
     * Default maximum amount of time an event handler is given to process event
     * before an exception is logged. this is used when an enduser does *not* define
     * the appropriate XML elements within the ACS CDB. see the inline doc on EventChannel.xsd
     * for more info.
     */
    static double DEFAULT_MAX_PROCESS_TIME;

    /**
     * Used to profile handler times.
     */
    Profiler *profiler_mp;
  private:

    /**
     * Utility method.
     * Initialization method where code common to all constructors is kept.
     * @param orb ORB which has a reference to the Naming Service.
     *  @htmlonly
     <br><hr>
     @endhtmlonly
    */
    void
    init(CORBA::ORB_ptr orb)
	throw (CORBAProblemEx);

    /**
     * ORB used by this consumer.
     */
    CORBA::ORB_ptr orb_mp;

    /**
     * ALMA C++ coding standards state assignment operators should be disabled.
     */
    void operator=(const Consumer&);

    /**
     * ALMA C++ coding standards state copy constructors should be disabled.
     */
    Consumer(const Consumer&);

    /** 
     *  Subscribe to more events.
     *  In all likelehood this method will become private very soon.
     *
     *  @param type_name Type of structured events to subscribe to.
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
    */
    void
    addSubscription(const char* type_name)
	throw (CORBAProblemEx);

    /** 
     *  Unsubscribe from events.
     *
     *  @param type_name Type of structured events to unsubscribe from.
     *  @return void
     *  @htmlonly
        <br><hr>
        @endhtmlonly
     */
    void
    removeSubscription(const char* type_name)
	throw (CORBAProblemEx);
    ///////////////////////////////////////////////////////////////////////////////////////
};
NAMESPACE_END(nc);

#endif /* CONSUMER_H */
