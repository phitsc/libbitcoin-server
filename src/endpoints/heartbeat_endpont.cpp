/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/endpoints/heartbeat_endpoint.hpp>

#include <cstdint>
#include <bitcoin/protocol.hpp>
#include <bitcoin/server/configuration.hpp>
#include <bitcoin/server/server_node.hpp>
#include <bitcoin/server/settings.hpp>
#include <bitcoin/server/utility/curve_authenticator.hpp>

namespace libbitcoin {
namespace server {

#define NAME "heartbeat_endpoint"

using std::placeholders::_1;
using namespace bc::protocol;

heartbeat_endpoint::heartbeat_endpoint(zmq::authenticator& authenticator,
    server_node* node)
  : counter_(rand()),
    settings_(node->server_settings()),
    socket_(authenticator, zmq::socket::role::publisher),
    deadline_(std::make_shared<deadline>(node->thread_pool(),
        settings_.heartbeat_interval()))
{
    if (!settings_.heartbeat_endpoint_enabled)
        return;

    const auto secure = settings_.server_private_key;

    if (!authenticator.apply(socket_, NAME, secure))
        socket_.stop();
}

bool heartbeat_endpoint::start()
{
    if (!settings_.heartbeat_endpoint_enabled)
    {
        stop();
        return true;
    }

    const auto heartbeat_endpoint = settings_.heartbeat_endpoint;

    if (!socket_ || !socket_.bind(heartbeat_endpoint))
    {
        log::error(LOG_ENDPOINT)
            << "Failed to initialize heartbeat service on " << heartbeat_endpoint;
        stop();
        return false;
    }

    log::info(LOG_ENDPOINT)
        << "Bound heartbeat service on " << heartbeat_endpoint;

    start_timer();
    return true;
}

bool heartbeat_endpoint::stop()
{
    deadline_->stop();
    return socket_.stop();
}

void heartbeat_endpoint::start_timer()
{
    deadline_->start(
        std::bind(&heartbeat_endpoint::send,
            this, _1));
}

void heartbeat_endpoint::send(const code& ec)
{
    if (ec)
    {
        log::error(LOG_ENDPOINT)
            << "Heartbeat timer failed: " << ec.message();
        return;
    }

    zmq::message message;
    message.enqueue_little_endian(counter_);
    const auto result = message.send(socket_);

    if (result)
        log::debug(LOG_ENDPOINT)
            << "Heartbeat sent: " << counter_;
    else
        log::debug(LOG_ENDPOINT)
            << "Heartbeat failed to send.";

    ++counter_;
    start_timer();
}

} // namespace server
} // namespace libbitcoin
