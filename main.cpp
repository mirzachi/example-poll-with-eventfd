#include <iostream>

#include <unistd.h>
#include <poll.h>
#include <sys/eventfd.h>

#include <array>
#include <atomic>
#include <functional>
#include <thread>
#include <csignal>
#include <cstring>

/**
 * \brief  Initialize the signal handler
 */
std::unique_ptr<std::thread> initializeSignalHandler(std::atomic<bool>& exitRequested)
{
    bool success{true};
    sigset_t signal_set;

    std::cout << "Set up signal handler..." << std::endl;

    /* Block all signals except the SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV signals because blocking them will lead to
     * undefined behavior. Their default handling shall not be changed (dependent on underlying POSIX environment, usually
     * process is killed and a dump file is written). Signal mask will be inherited by subsequent threads. */
    success = success && (0 == sigfillset(&signal_set));
    success = success && (0 == sigdelset(&signal_set, SIGABRT));
    success = success && (0 == sigdelset(&signal_set, SIGBUS));
    success = success && (0 == sigdelset(&signal_set, SIGFPE));
    success = success && (0 == sigdelset(&signal_set, SIGILL));
    success = success && (0 == sigdelset(&signal_set, SIGSEGV));
    success = success && (0 == pthread_sigmask(SIG_SETMASK, &signal_set, nullptr));

    if (!success)
    {
        std::cout << "Setting up signal handler failed" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    /* spawn a new signal handler thread */
    auto signalHandlerThread = std::make_unique<std::thread>([&exitRequested]() {
        // set the the thread name to facilitate debugging
        const std::string signalHandlerThreadName{"SignalHandler"};
        pthread_setname_np(pthread_self(), signalHandlerThreadName.c_str());
        sigset_t signal_set;
        std::int32_t sig{-1};
        /* Empty the set of signals. */
        if (0 != sigemptyset(&signal_set))
        {
            std::cout << "Empty signal set failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        /* Add SIGINT (CTRL+C) to the set of signals. */
        if (0 != sigaddset(&signal_set, SIGINT))
        {
            std::cout << "Add SIGINT failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        /* Add SIGTSTP (CTRL+Z) to the set of signals. */
        if (0 != sigaddset(&signal_set, SIGTSTP))
        {
            std::cout << "Add SIGTSTP failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        /* Add SIGTERM (default signal sent by kill) to the set of signals. */
        if (0 != sigaddset(&signal_set, SIGTERM))
        {
            std::cout << "Add SIGTERM failed" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        while (!(sig == SIGINT || sig == SIGTSTP || sig == SIGTERM))
        {
            sigwait(&signal_set, &sig);
        }
        exitRequested = true;
    });

    return signalHandlerThread;
}

/**
 * \brief  Deinitialize the signal handler
 */
void deinitializeSignalHandler(std::unique_ptr<std::thread>&& signalHandlerThread)
{
    if (signalHandlerThread->joinable())
    {
        // send the SIGINT signal to signalHandlerThread if it exists
        // allows shutting down the daemon without using Ctrl+C
        // e.g. when loading of agents fails
        auto ret = pthread_kill(signalHandlerThread->native_handle(), SIGINT);
        std::cout << "pthread_kill return: " << std::string(strerror(ret))  << std::endl;
        signalHandlerThread->join();
    }
}

int main() {
    // set in a signal handler
    std::atomic_bool exitRequested(false);
    // Initialize signal handler
    auto signalHandlerThread = initializeSignalHandler(exitRequested);

    constexpr int nfds{2};
    std::array<pollfd, nfds> pfds{};

    auto eventFd1 = eventfd(0, EFD_NONBLOCK);
    auto eventFd2 = eventfd(0, EFD_NONBLOCK);

    pfds[0].fd = eventFd1;
    pfds[0].events = POLLIN;

    pfds[1].fd = eventFd2;
    pfds[1].events = POLLIN;

    std::thread sender1([&eventFd1, &exitRequested](){
        uint64_t tmp{1};
        while (!exitRequested){
            write(eventFd1, &tmp, sizeof(uint64_t));
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::thread sender2([&eventFd2, &exitRequested](){
        uint64_t tmp{2};
        while (!exitRequested){
            write(eventFd2, &tmp, sizeof(uint64_t));
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    while(!exitRequested) {
        int ready;

        std::cout << "About to poll..." << std::endl;
        ready = poll(pfds.data(), nfds, 500); // milliseconds
        if (ready == -1){
            std::cout << "Poll error!" << std::endl;
            exit(1);
        }

        for (int i = 0; i < nfds; ++i) {
            if (pfds[i].revents & POLLIN){
                char  buf[10];
                ssize_t signal = read(pfds[i].fd, buf, sizeof(buf));
                if (signal == -1){
                    std::cout << "Read error!" << std::endl;
                    exit(1);
                }
                std::cout << "Received from Sender " << *reinterpret_cast<uint64_t*>(buf) << std::endl;
            }
        }

    }

    sender1.join();
    sender2.join();
    deinitializeSignalHandler(std::move(signalHandlerThread));
    close(eventFd1);
    close(eventFd2);

    return 0;
}
