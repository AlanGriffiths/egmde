/*
 * Copyright © 2016 Octopull Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef EGMDE_EGWALLPAPER_H
#define EGMDE_EGWALLPAPER_H

#include <mir/client/connection.h>
#include <mir/client/window.h>

#include <miral/application.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>

class Worker
{
public:
    ~Worker();

    void start_work();
    void enqueue_work(std::function<void()> const& functor);
    void stop_work();

private:
    using WorkQueue = std::queue<std::function<void()>>;

    std::mutex mutable work_mutex;
    std::condition_variable work_cv;
    WorkQueue work_queue;
    bool work_done = false;
    std::thread worker;

    void do_work();
};

class Wallpaper : Worker
{
public:
    // These operators are the protocol for an "Internal Client"
    void operator()(mir::client::Connection c) { start(c); }
    void operator()(std::weak_ptr<mir::scene::Session> const&){ }

    void start(mir::client::Connection connection);
    void stop();

private:

    std::mutex mutable mutex;
    mir::client::Connection connection;
    mir::client::Window surface;

    void create_surface();
};


#endif //EGMDE_EGWALLPAPER_H
