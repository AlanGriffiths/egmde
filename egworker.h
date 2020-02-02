/*
 * Copyright © 2018 Octopull Limited
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

#ifndef EGMDE_EGWORKER_H
#define EGMDE_EGWORKER_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace egmde
{
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

    void do_work();
};
}

#endif //EGMDE_EGWORKER_H
