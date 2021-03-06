#include "applicationmanager.h"

#include "utils/logger.h"
#include "console/consolehandler.h"
#include "plugins/pluginmanager.h"
#include "network/clientsession.h"
#include "network/networkmanager.h"
ApplicationManager ApplicationManager::_instance;

void ApplicationManager::Init()
{
    ConsoleHandler::getInstance().moveToThread(&_consoleThread);

    LOG_INFO("Initialisation des connexions signaux/slots...");

    // -- initialisation des connexions pour la communication inter-threads
    connect(&_consoleThread, &QThread::started, &ConsoleHandler::getInstance(), &ConsoleHandler::Slot_init);

    // --- console_handler --> application_manager
    connect(&(ConsoleHandler::getInstance()), SIGNAL(sig_state()),                 SLOT(Slot_state()));
    connect(&(ConsoleHandler::getInstance()), SIGNAL(sig_shutdown()),              SLOT(Slot_shutdown()));
    connect(&(ConsoleHandler::getInstance()), SIGNAL(sig_terminated()),            SLOT(Slot_terminated()));
    connect(&(ConsoleHandler::getInstance()), SIGNAL(sig_connect()),        SLOT(Slot_connect()));
    // --- application_mgr --> console_handler
    connect(this, SIGNAL(sig_response(Command,bool,QString)),
            &(ConsoleHandler::getInstance()), SLOT(Slot_response(Command,bool,QString)));
    // --- plugin_mgr --> application_mgr
    connect(&(PluginManager::getInstance()), SIGNAL(sig_terminated()), SLOT(Slot_terminated()));
    // --- application_mgr --> plugin_mgr
    connect(this, SIGNAL(sig_terminateModule()),
            &(PluginManager::getInstance()), SLOT(Slot_terminate()));

    LOG_INFO("Initialisation des composants...");
    // -- initialisation des composants
    PluginManager::getInstance().Init();
    if(!PluginManager::getInstance().CheckPlugins())
    {   LOG_CRITICAL("Plugins integrity check failed !");
    }
    _clientSession = NULL;
    _consoleThread.start();
}

void ApplicationManager::Slot_state()
{
    QString report = "\n"
                     "----------------- CLIENT STATE REPORT -----------------\n"
                     "\n"
                     "Available plugins :\n";
    QStringList plugins = PluginManager::getInstance().GetPluginsList();
    if(plugins.size() > 0)
    {   foreach (QString plugin, plugins)
        {   report += QString("  + %1\n").arg(plugin);
        }
    }
    else
    {   report += "  - no plugin available, this client is useless.\n";
    }
    if( _clientSession != NULL)
    {
        report += QString("\n"
                "State : CONNECTED \n"
                "ID : %1\n").arg(_clientSession->Id());
        if(_clientSession->GetCurrentCalculation() != NULL)
        {
            report += "\n"
                    "Calculation Type : %1\n"
                    "Calculation ID : %2\n"
                    "Fragment ID : %3\n";
            report.arg(_clientSession->GetCurrentCalculation()->GetBin(),
                       _clientSession->GetCurrentCalculation()->GetId().toString(),
                       QString(_clientSession->FragmentId()));
        }
        else
        {
            report += "No calculation.";
        }
    }
    else
    {
        report += "\n"
                "STATE: NOT CONNECTED\n";
    }

    LOG_DEBUG("sig_response(CMD_STATE) emitted.");
    emit sig_response(CMD_STATE, true, report);
}


void ApplicationManager::Slot_shutdown()
{   LOG_DEBUG("Slot_shutdown() called.");
    emit sig_terminateModule();
#if TERMINATED_EXPECTED_TOTAL == 1
    if(TERMINATED_EXPECTED_TOTAL - _terminated_ctr == 1)
    {   delete _clientSession;
        LOG_DEBUG("sig_response(CMD_SHUTDOWN) emitted.");
        emit sig_response(CMD_SHUTDOWN, true, QString("SHUTDOWN command received !"));
    }
#endif
}

void ApplicationManager::Slot_terminated()
{   LOG_DEBUG("Slot_terminated() called.");
    _terminated_ctr++;
    if(_terminated_ctr >= TERMINATED_EXPECTED_TOTAL)
    {   LOG_DEBUG("sig_terminated() emitted.");
        _consoleThread.quit();
        _consoleThread.wait();
        emit sig_terminated();
    }
    else if(TERMINATED_EXPECTED_TOTAL - _terminated_ctr == 1)
    {   LOG_DEBUG("sig_response(CMD_SHUTDOWN) emitted.");
        emit sig_response(CMD_SHUTDOWN, true, QString("SHUTDOWN command received !"));
    }
    // emission du signal de terminaison quand tous les composants attendus ont notofié l'app manager de leur terminaison
}
void ApplicationManager::Slot_connect()
{
    LOG_DEBUG("Slot_connect() called.");
    QString report = "";
    //report += "not implemented yet\n";
    //TODO gerer si le client fait 2 fois CONNECT (à voir avec le multi server)
    _clientSession = new ClientSession();
    LOG_DEBUG("sig_response(CMD_CONNECT) emitted.");
    emit sig_response(CMD_CONNECT, true, report);
}

ApplicationManager::ApplicationManager() :
    _terminated_ctr(0)
{
}

ApplicationManager::~ApplicationManager()
{

}
