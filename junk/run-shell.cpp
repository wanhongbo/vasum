#include <unistd.h>
#include <iostream>
#include <vector>
#include <lxcpp/lxcpp.hpp>
#include <lxcpp/logger-config.hpp>
#include <logger/logger.hpp>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

using namespace lxcpp;

pid_t initPid = -1;

void sighandler(int signal)
{
    // remove the log in a deffered manner
    pid_t pid = fork();

    if (pid == 0)
    {
        pid_t pid = fork();

        if (pid == 0)
        {
            if (initPid > 0)
                ::kill(initPid, SIGTERM);
            sleep(11);
            ::unlink("/tmp/lxcpp-shell.txt");

            exit(0);
        }

        exit(0);
    }

    wait();
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, &sighandler);

    logger::setupLogger(logger::LogType::LOG_STDERR, logger::LogLevel::TRACE);
    LOGT("Color test: TRACE");
    LOGD("Color test: DEBUG");
    LOGI("Color test: INFO");
    LOGW("Color test: WARN");
    LOGE("Color test: ERROR");

    logger::setupLogger(logger::LogType::LOG_STDERR, logger::LogLevel::DEBUG);
    //logger::setupLogger(logger::LogType::LOG_FILE, logger::LogLevel::TRACE, "/tmp/lxcpp-shell.txt");

    std::vector<std::string> args;
    args.push_back("/bin/bash");

    try
    {
        Container* c = createContainer("test", "/");
        c->setInit(args);
        c->setLogger(logger::LogType::LOG_FILE, logger::LogLevel::TRACE, "/tmp/lxcpp-shell.txt");
        c->setTerminalCount(4);
        c->start();
        c->console();
        // You could run the console for the second time to see if it can be reattached
        //c->console();

        initPid = c->getInitPid();

        delete c;
    }
    catch (const std::exception &e)
    {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
    }

    sighandler(3);

    return 0;
}
