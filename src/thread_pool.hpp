// Copyright (C) 2020 averne
//
// This file is part of fuse-nx.
//
// fuse-nx is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// fuse-nx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fuse-nx.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace fnx {

template <typename Params>
class ThreadPool {
    public:
        template <typename F>
        ThreadPool(F &&worker_func): worker_func(worker_func) { }

        ~ThreadPool() {
            if (!this->exited)
                this->stop_workers();
        }

        void start_workers(std::size_t num_workers) {
            num_workers = std::max(1ul, num_workers);

            this->workers.reserve(num_workers);
            for (std::size_t i = 0; i < num_workers; ++i)
                this->workers.emplace_back(std::thread(&ThreadPool::worker_thread_func, this));
        }

        void stop_workers() {
            this->is_exiting = true;
            this->condvar.notify_all();
            for (auto &t: this->workers)
                t.join();
            this->exited = true;
        }

        void wait() {
            while (!this->pending_args.empty())
                std::this_thread::yield();
        }

        std::size_t get_num_workers() const {
            return this->workers.size();
        }

        void queue_item(const Params &args) {
            std::scoped_lock lk(this->pending_args_mtx);
            this->pending_args.push(args);
            this->condvar.notify_one();
        }

    private:
        void worker_thread_func() {
            while (!this->is_exiting) {
                std::unique_lock lk(this->pending_args_mtx);
                this->condvar.wait(lk,
                    [this] { return this->is_exiting || !this->pending_args.empty(); });

                if (this->is_exiting)
                    return;

                Params args = std::move(this->pending_args.front());
                this->pending_args.pop();
                lk.unlock();

                this->worker_func(std::move(args));
            }
        }

    private:
        bool exited = false;

        std::function<void(Params)> worker_func;
        std::vector<std::thread>    workers;

        std::atomic_bool        is_exiting = false;
        std::condition_variable condvar;

        std::mutex         pending_args_mtx;
        std::queue<Params> pending_args;
};

} // namespace fnx
