/**
 * Copyright (c) 2011-2016 libbitcoin developers (see AUTHORS)
 *
 * This file is part of metaverse.
 *
 * metaverse is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <metaverse/network/p2p.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <metaverse/bitcoin.hpp>
#include <metaverse/network/channel.hpp>
#include <metaverse/network/connections.hpp>
#include <metaverse/network/define.hpp>
#include <metaverse/network/hosts.hpp>
#include <metaverse/network/protocols/protocol_address.hpp>
#include <metaverse/network/protocols/protocol_ping.hpp>
#include <metaverse/network/protocols/protocol_seed.hpp>
#include <metaverse/network/protocols/protocol_version.hpp>
#include <metaverse/network/sessions/session_inbound.hpp>
#include <metaverse/network/sessions/session_manual.hpp>
#include <metaverse/network/sessions/session_outbound.hpp>
#include <metaverse/network/sessions/session_seed.hpp>
#include <metaverse/network/settings.hpp>

namespace libbitcoin {
namespace network {

#define NAME "p2p"

using namespace std::placeholders;

p2p::p2p(const settings& settings)
  : settings_(settings),
    stopped_(true),
    height_(0),
    hosts_(std::make_shared<hosts>(threadpool_, settings_)),
    connections_(std::make_shared<connections>()),
    stop_subscriber_(std::make_shared<stop_subscriber>(threadpool_, NAME "_stop_sub")),
    channel_subscriber_(std::make_shared<channel_subscriber>(threadpool_, NAME "_sub"))
{
}

// This allows for shutdown based on destruct without need to call stop.
p2p::~p2p()
{
    p2p::close();
}

// Start sequence.
// ----------------------------------------------------------------------------

void p2p::start(result_handler handler)
{
    if (!stopped())
    {
        handler(error::operation_failed);
        return;
    }

    threadpool_.join();
    threadpool_.spawn(settings_.threads, thread_priority::low);

    stopped_ = false;
    stop_subscriber_->start();
    channel_subscriber_->start();

    // This instance is retained by stop handler and member references.
    const auto manual = attach_manual_session();
    manual_.store(manual);

    // This is invoked on a new thread.
    manual->start(
        std::bind(&p2p::handle_manual_started,
            this, _1, handler));
}

void p2p::handle_manual_started(const code& ec, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Error starting manual session: " << ec.message();
        handler(ec);
        return;
    }

    handle_hosts_loaded(hosts_->start(), handler);
}

void p2p::handle_hosts_loaded(const code& ec, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Error loading host addresses: " << ec.message();
        handler(ec);
        return;
    }

    // The instance is retained by the stop handler (until shutdown).
    const auto seed = attach_seed_session();

    // This is invoked on a new thread.
    seed->start(
        std::bind(&p2p::handle_started,
            this, _1, handler));
}

void p2p::handle_started(const code& ec, result_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Error seeding host addresses: " << ec.message();
        handler(ec);
        return;
    }

    // There is no way to guarantee subscription before handler execution.
    // So currently subscription for seed node connections is not supported.
    // Subscription after this return will capture connections established via
    // subsequent "run" and "connect" calls, and will clear on close/destruct.

    // This is the end of the start sequence.
    handler(error::success);
}

// Run sequence.
// ----------------------------------------------------------------------------

void p2p::run(result_handler handler)
{
    // Start node.peer persistent connections.
    for (const auto& peer: settings_.peers)
        connect(peer);

    // The instance is retained by the stop handler (until shutdown).
    const auto inbound = attach_inbound_session();

    // This is invoked on a new thread.
    inbound->start(
        std::bind(&p2p::handle_inbound_started,
            this, _1, handler));
}

void p2p::handle_inbound_started(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Error starting inbound session: " << ec.message();
        handler(ec);
        return;
    }
    
    // The instance is retained by the stop handler (until shutdown).
    const auto outbound = attach_outbound_session();

    // This is invoked on a new thread.
    outbound->start(
        std::bind(&p2p::handle_running,
            this, _1, handler));
}

void p2p::handle_running(const code& ec, result_handler handler)
{
    if (ec)
    {
        log::error(LOG_NETWORK)
            << "Error starting outbound session: " << ec.message();
        handler(ec);
        return;
    }

    // This is the end of the run sequence.
    handler(error::success);
}

// Specializations.
// ----------------------------------------------------------------------------
// Create derived sessions and override these to inject from derived p2p class.

session_seed::ptr p2p::attach_seed_session()
{
    return attach<session_seed>();
}

session_manual::ptr p2p::attach_manual_session()
{
    return attach<session_manual>();
}

session_inbound::ptr p2p::attach_inbound_session()
{
    return attach<session_inbound>();
}

session_outbound::ptr p2p::attach_outbound_session()
{
    return attach<session_outbound>();
}

// Shutdown.
// ----------------------------------------------------------------------------
// All shutdown actions must be queued by the end of the stop call.
// IOW queued shutdown operations must not enqueue additional work.

// This is not short-circuited by a stopped test because we need to ensure it
// completes at least once before returning. This requires a unique lock be 
// taken around the entire section, which poses a deadlock risk. Instead this
// is thread safe and idempotent, allowing it to be unguarded.
bool p2p::stop()
{
    // This is the only stop operation that can fail.
    const auto result = (hosts_->stop() == (code)error::success);

    // Signal all current work to stop and free manual session.
    stopped_ = true;
    manual_.store(nullptr);

    // Prevent subscription after stop.
    stop_subscriber_->stop();
    stop_subscriber_->invoke(error::service_stopped);

    // Prevent subscription after stop.
    channel_subscriber_->stop();
    channel_subscriber_->invoke(error::service_stopped, nullptr);

    // Stop accepting channels and stop those that exist (self-clearing).
    connections_->stop(error::service_stopped);

    // Signal threadpool to stop accepting work now that subscribers are clear.
    threadpool_.shutdown();
    return result;
}

// This must be called from the thread that constructed this class (see join).
bool p2p::close()
{
    // Signal current work to stop and threadpool to stop accepting new work.
    const auto result = p2p::stop();

    // Block on join of all threads in the threadpool.
    threadpool_.join();
    return result;
}

// Properties.
// ----------------------------------------------------------------------------

const settings& p2p::network_settings() const
{
    return settings_;
}

// The blockchain height is set in our version message for handshake.
size_t p2p::height() const
{
    return height_;
}

// The height is set externally and is safe as an atomic.
void p2p::set_height(size_t value)
{
    height_ = value;
}

bool p2p::stopped() const
{
    return stopped_;
}

threadpool& p2p::thread_pool()
{
    return threadpool_;
}

// Subscriptions.
// ----------------------------------------------------------------------------

void p2p::subscribe_connection(connect_handler handler)
{
    channel_subscriber_->subscribe(handler, error::service_stopped, nullptr);
}

void p2p::subscribe_stop(result_handler handler)
{
    stop_subscriber_->subscribe(handler, error::service_stopped);
}

// Manual connections.
// ----------------------------------------------------------------------------

void p2p::connect(const config::endpoint& peer)
{
    connect(peer.host(), peer.port());
}

void p2p::connect(const std::string& hostname, uint16_t port)
{
    if (stopped())
        return;

    auto manual = manual_.load();
    if (manual)
        manual->connect(hostname, port);
}

void p2p::connect(const std::string& hostname, uint16_t port,
    channel_handler handler)
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    auto manual = manual_.load();
    if (manual)
    {
        // Connect is invoked on a new thread.
        manual->connect(hostname, port, handler);
    }
}

// Connections collection.
// ----------------------------------------------------------------------------

void p2p::connected(const address& address, truth_handler handler)
{
    connections_->exists(address, handler);
}

void p2p::store(channel::ptr channel, result_handler handler)
{
    const auto new_connection_handler =
        std::bind(&p2p::handle_new_connection,
            this, _1, channel, handler);

    connections_->store(channel, new_connection_handler);
}

void p2p::handle_new_connection(const code& ec, channel::ptr channel,
    result_handler handler)
{
    // Connection-in-use indicated here by error::address_in_use.
    handler(ec);
    
    if (!ec && channel->notify())
        channel_subscriber_->relay(error::success, channel);
}

void p2p::remove(channel::ptr channel, result_handler handler)
{
    connections_->remove(channel, handler);
}

void p2p::connected_count(count_handler handler)
{
    connections_->count(handler);
}

// Hosts collection.
// ----------------------------------------------------------------------------

void p2p::fetch_address(const config::authority::list& excluded_list, address_handler handler)
{
    address out;
    handler(hosts_->fetch(out, excluded_list), out);
}

config::authority::list p2p::authority_list()
{
    return connections_->authority_list();
}

void p2p::store(const address& address, result_handler handler)
{
    handler(hosts_->store(address));
}

void p2p::store(const address::list& addresses, result_handler handler)
{
    // Store is invoked on a new thread.
    hosts_->store(addresses, handler);
}

void p2p::remove(const address& address, result_handler handler)
{
    handler(hosts_->remove(address));
}

void p2p::address_count(count_handler handler)
{
    handler(hosts_->count());
}

p2p::address::list p2p::address_list()
{
	return hosts_->copy();
}

connections::ptr p2p::connections_ptr()
{
	return connections_;
}
} // namespace network
} // namespace libbitcoin
