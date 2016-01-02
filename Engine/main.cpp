/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2015 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QDesktopWidget>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>

#include <QtDebug>

#include <iostream>
#include "version.h"

#include "common_features/app_path.h"
#include "common_features/graphics_funcs.h"
#include "common_features/logger.h"
#include "common_features/episode_state.h"
#include "common_features/translator.h"

#include <settings/global_settings.h>

#include "data_configs/select_config.h"
#include "data_configs/config_manager.h"

#include "graphics/window.h"
#include "graphics/gl_renderer.h"

#include <audio/SdlMusPlayer.h>

#include <PGE_File_Formats/file_formats.h>
#include <PGE_File_Formats/smbx64.h>
#include "fontman/font_manager.h"
#include "gui/pge_msgbox.h"
#include "networking/intproc.h"
#include "graphics/graphics.h"

#include <settings/global_settings.h>

#include "scenes/scene_level.h"
#include "scenes/scene_world.h"
#include "scenes/scene_loading.h"
#include "scenes/scene_title.h"
#include "scenes/scene_credits.h"
#include "scenes/scene_gameover.h"

#include <QMessageBox>

#include <cstdlib>
#include <ctime>
#include <iostream>

/* Signals Begin */
#include <signal.h>

void crashByUnhandledException()
{
    WriteToLog(QtWarningMsg, "<Unhandled exception!>");
    QMessageBox::critical(NULL, QObject::tr("Unhandled exception!"),
                 QObject::tr("Engine was crashed because accepted unhandled exception!"));
    exit(-1);
}

void crashByFlood()
{
    WriteToLog(QtWarningMsg, "<Out of memory!>");
    QMessageBox::critical(NULL, QObject::tr("Out of memory!"),
                 QObject::tr("Engine was crashed because out of memory! Try to close other applications and restart game."));
    exit(-1);
}


void handle_signal(int signal)
{
    // Find out which signal we're handling
    switch (signal) {
        #ifndef _WIN32  //Unsupported signals by Windows
        case SIGHUP:
            WriteToLog(QtWarningMsg, "Terminal was closed");
            exit(signal);
            break;
        case SIGQUIT:
            WriteToLog(QtWarningMsg, "<Quit command>");
            exit(signal);
        case SIGKILL:
            WriteToLog(QtWarningMsg, "<killed>");
            QMessageBox::critical(NULL, QObject::tr("Killed!"),
                         QObject::tr("Engine was killed by mad maniac :-P"));
            exit(signal);
            break;
        case SIGALRM:
            WriteToLog(QtWarningMsg, "<alarm() time out!>");
            QMessageBox::critical(NULL, QObject::tr("Time out!"),
                          QObject::tr("Engine was abourted because alarm() time out!"));
            exit(signal);
            break;
        case SIGURG:
        case SIGUSR1:
        case SIGUSR2:
            break;
        case SIGILL:
            WriteToLog(QtWarningMsg, "<Wrong CPU Instruction>");
            QMessageBox::critical(NULL, QObject::tr("Wrong CPU Instruction!"),
                         QObject::tr("Engine was crashed because a wrong CPU instruction"));
            exit(signal);
        #endif
        case SIGFPE:
            WriteToLog(QtWarningMsg, "<wrong arithmetical operation>");
            QMessageBox::critical(NULL, QObject::tr("Wrong arithmetical operation"),
                          QObject::tr("Engine was crashed because wrong arithmetical opreation!"));
            exit(signal);
            break;
        case SIGABRT:
            WriteToLog(QtWarningMsg, "<Aborted!>");
            QMessageBox::critical(NULL, QObject::tr("Aborted"),
                          QObject::tr("Engine was been aborted because critical error was occouped."));
            exit(signal);
        case SIGSEGV:
            WriteToLog(QtWarningMsg, "<Segmentation fault crash!>");
            QMessageBox::critical(NULL, QObject::tr("Segmentation fault"),
                          QObject::tr("Engine was crashed because Segmentation fault. Run debug with built in debug mode "
                                      "and retry your recent action to take more detail info."));
            exit(signal);
            break;
        case SIGINT:
            WriteToLog(QtWarningMsg, "<Interrupted!>");
            QMessageBox::critical(NULL, QObject::tr("Interrupt"),
                          QObject::tr("Engine was interrupted"));
            exit(0);
            break;
        default:
            return;
    }
}

void initSigs()
{
    std::set_new_handler(&crashByFlood);
    std::set_terminate(&crashByUnhandledException);
    #ifndef _WIN32 //Unsupported signals by Windows
    signal(SIGHUP, &handle_signal);
    signal(SIGQUIT, &handle_signal);
    signal(SIGKILL, &handle_signal);
    signal(SIGALRM, &handle_signal);
    signal(SIGURG, &handle_signal);
    signal(SIGUSR1, &handle_signal);
    signal(SIGUSR2, &handle_signal);
    #endif
    signal(SIGILL, &handle_signal);
    signal(SIGFPE, &handle_signal);
    signal(SIGSEGV, &handle_signal);
    signal(SIGINT, &handle_signal);
    signal(SIGABRT, &handle_signal);
}
/* Signals End */

enum Level_returnTo
{
    RETURN_TO_MAIN_MENU=0,
    RETURN_TO_WORLDMAP,
    RETURN_TO_GAMEOVER_SCREEN,
    RETURN_TO_CREDITS_SCREEN,
    RETURN_TO_EXIT
};
Level_returnTo end_level_jump=RETURN_TO_EXIT;

struct cmdArgs
{
    cmdArgs()
    {
        debugMode=false;
        testWorld=false;
        testLevel=false;
    }
    bool debugMode;
    bool testWorld;
    bool testLevel;
} _flags;

int main(int argc, char *argv[])
{
    initSigs();

    srand( std::time(NULL) );

    QApplication::addLibraryPath( "." );
    QApplication::addLibraryPath( QFileInfo(QString::fromUtf8(argv[0])).dir().path() );
    QApplication::addLibraryPath( QFileInfo(QString::fromLocal8Bit(argv[0])).dir().path() );

    QApplication a(argc, argv);

    #ifdef Q_OS_LINUX
    a.setStyle("GTK");
    #endif

    ///Generating application path

    //Init system paths
    AppPathManager::initAppPath();

    PGE_Translator translator;
    translator.init(&a);

    foreach(QString arg, a.arguments())
    {
        if(arg=="--install")
        {
            AppPathManager::install();
            AppPathManager::initAppPath();

            QApplication::quit();
            QApplication::exit();

            return 0;
        } else if(arg=="--version") {
            std::cout << _INTERNAL_NAME " " _FILE_VERSION << _FILE_RELEASE "-" _BUILD_VER << std::endl;
            QApplication::quit();
            QApplication::exit();
            return 0;
        }
    }

    g_AppSettings.load();
    g_AppSettings.apply();

    //Init log writer
    LoadLogSettings();

    QString configPath="";
    QString fileToOpen = "";
    PlayEpisodeResult episode;
    episode.character=0;
    episode.savefile="save1.savx";
    episode.worldfile="";
    g_AppSettings.debugMode=false; //enable debug mode
    g_AppSettings.interprocessing=false; //enable interprocessing

    bool skipFirst=true;
    foreach(QString param, a.arguments())
    {
        if(skipFirst) {skipFirst=false; continue;}
        qDebug() << param;

        if(param.startsWith("--config="))
        {
            QStringList tmp;
            tmp = param.split('=');
            if(tmp.size()>1)
            {
                configPath = tmp.last();
                if(!SMBX64::qStr(configPath))
                {
                    configPath = FileFormats::removeQuotes(configPath);
                }
            }
        }
        else
        if(param == ("--debug"))
        {
            g_AppSettings.debugMode=true;
            _flags.debugMode=true;
        }
        else
        if(param == ("--interprocessing"))
        {
            IntProc::init();
            g_AppSettings.interprocessing=true;
        }
        else
        {
            fileToOpen = param;
        }
    }


    ////Check & ask for configuration pack


    //Create empty config directory if not exists
    if(!QDir(AppPathManager::userAppDir() + "/" +  "configs").exists())
        QDir().mkdir(AppPathManager::userAppDir() + "/" +  "configs");

    // Config manager
    SelectConfig *cmanager = new SelectConfig();
    cmanager->setWindowFlags (Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    cmanager->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, cmanager->size(), a.desktop()->availableGeometry() ));

    QString configPath_manager = cmanager->isPreLoaded();

    //If application runned first time or target configuration is not exist
    if(configPath_manager.isEmpty() && configPath.isEmpty())
    {
        //Ask for configuration
        if(cmanager->exec()==QDialog::Accepted)
        {
            configPath = cmanager->currentConfigPath;
        }
        else
        {
            delete cmanager;
            IntProc::quit();
            exit(0);
        }
    } else if(!configPath_manager.isEmpty() && configPath.isEmpty()) {
        configPath = cmanager->currentConfigPath;
    }
    delete cmanager;



    //Load selected configuration pack

    WriteToLog(QtDebugMsg, "Opening of the configuration package...");
    ConfigManager::setConfigPath(configPath);

    WriteToLog(QtDebugMsg, "Initalization of basic properties...");
    if(!ConfigManager::loadBasics()) exit(1);

    WriteToLog(QtDebugMsg, "Configuration package successfully loaded!");

    // Initalizing SDL
    WriteToLog(QtDebugMsg, "Initialization of SDL...");
    if ( SDL_Init(SDL_INIT_EVERYTHING) < 0 )
    {
        QMessageBox::critical(NULL, "SDL Error",
            QString("Unable to init SDL!\n%1")
            .arg( SDL_GetError() ), QMessageBox::Ok);
            //std::cout << "Unable to init SDL, error: " << SDL_GetError() << '\n';
        exit(1);
    }

    WriteToLog(QtDebugMsg, "Initialization of Audio subsystem...");
    if(PGE_MusPlayer::initAudio(44100, 32, 4096)==-1)
    {
        QMessageBox::critical(NULL, "Audio subsystem Error",
            QString("Unable to load audio sub-system!\n%1")
                .arg( Mix_GetError() ), QMessageBox::Ok);
        exit(1);
    }
    PGE_MusPlayer::MUS_changeVolume(g_AppSettings.volume_music);

    WriteToLog(QtDebugMsg, "Build SFX index cache...");
    ConfigManager::buildSoundIndex(); //Load all sound effects into memory

    WriteToLog(QtDebugMsg, "Init main window...");
    if(!PGE_Window::init(QString("Platformer Game Engine - v")+_FILE_VERSION+_FILE_RELEASE+" build "+_BUILD_VER)) exit(1);

    WriteToLog(QtDebugMsg, "Init joystics...");
    g_AppSettings.initJoysticks();
    g_AppSettings.loadJoystickSettings();

    WriteToLog(QtDebugMsg, "Clear screen...");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glLoadIdentity();//Reset modelview matrix
    glFlush();
    SDL_GL_SwapWindow(PGE_Window::window);

    if(g_AppSettings.fullScreen) qDebug()<<"Toggle fullscreen...";
    PGE_Window::setFullScreen(g_AppSettings.fullScreen);
    GlRenderer::resetViewport();

    //Init font manager
    WriteToLog(QtDebugMsg, "Init font manager...");
    FontManager::init();

    WriteToLog(QtDebugMsg, "Showing window...");
    SDL_ShowWindow(PGE_Window::window);
    SDL_PumpEvents();

    EpisodeState _game_state;

if(!fileToOpen.isEmpty())
{
    _game_state.reset();

    if(
       (fileToOpen.endsWith(".lvl", Qt::CaseInsensitive))
            ||
       (fileToOpen.endsWith(".lvlx", Qt::CaseInsensitive)))
    {
        _game_state.LevelFile = fileToOpen;
        _game_state.isEpisode = false;

        _flags.testLevel=true;
        _flags.testWorld=false;
        goto PlayLevel;
    }
    else
    if(
       (fileToOpen.endsWith(".wld", Qt::CaseInsensitive))
            ||
       (fileToOpen.endsWith(".wldx", Qt::CaseInsensitive)))
    {
        episode.character=1;
        episode.savefile="save1.savx";
        episode.worldfile=fileToOpen;
        _game_state._episodePath= QFileInfo(fileToOpen).absoluteDir().absolutePath()+"/";
        _game_state.saveFileName = episode.savefile;
        _game_state.isEpisode = true;
        _game_state.WorldFile = fileToOpen;

        _flags.testLevel=false;
        _flags.testWorld=true;
        goto PlayWorldMap;
    }
}

if(g_AppSettings.interprocessing) goto PlayLevel;

LoadingScreen:
{
    LoadingScene *ttl = new LoadingScene;
    ttl->setWaitTime(15000);

    ttl->init();
    ttl->fader.setFade(10, 0.0f, 0.01f);
    int ret = ttl->exec();
    if(ttl->doShutDown()) ret=-1;
    delete ttl;
    if(ret==-1) goto ExitFromApplication;

    goto MainMenu;
}

CreditsScreen:
{
    CreditsScene *ttl = new CreditsScene;
    ttl->setWaitTime(30000);

    ttl->init();
    ttl->fader.setFade(10, 0.0f, 0.01f);
    int ret = ttl->exec();
    if(ttl->doShutDown()) ret=-1;
    delete ttl;
    if(ret==-1) goto ExitFromApplication;

    if(_flags.testWorld)
        goto ExitFromApplication;

    goto MainMenu;
}

GameOverScreen:
{
    GameOverScene GOScene;
    int result = GOScene.exec();
    if (result == GameOverSceneResult::CONTINUE)
    {
        if (_game_state.isHubLevel)
            goto PlayLevel;
        else
            goto PlayWorldMap;
    }

    if(_flags.testWorld)
        goto ExitFromApplication;

    goto MainMenu;
}

MainMenu:
{
    _game_state.reset();
    TitleScene * iScene = new TitleScene();
    iScene->init();
    iScene->fader.setFade(10, 0.0f, 0.02f);
    int answer = iScene->exec();
    PlayLevelResult   res_level   = iScene->result_level;
    PlayEpisodeResult res_episode = iScene->result_episode;
    if(iScene->doShutDown()) answer=TitleScene::ANSWER_EXIT;
    delete iScene;

    switch(answer)
    {
        case TitleScene::ANSWER_EXIT:
            goto ExitFromApplication;
        case TitleScene::ANSWER_CREDITS:
            goto CreditsScreen;
        case TitleScene::ANSWER_LOADING:
            goto LoadingScreen;
        case TitleScene::ANSWER_GAMEOVER:
            goto GameOverScreen;
        case TitleScene::ANSWER_PLAYLEVEL:
            end_level_jump=RETURN_TO_MAIN_MENU;
            _game_state.isEpisode=false;
            _game_state.numOfPlayers=1;
            _game_state.LevelFile = res_level.levelfile;
            _game_state._episodePath.clear();
            _game_state.saveFileName.clear();
            goto PlayLevel;
        case TitleScene::ANSWER_PLAYEPISODE:
        case TitleScene::ANSWER_PLAYEPISODE_2P:
            end_level_jump=RETURN_TO_WORLDMAP;
            _game_state.numOfPlayers=(answer==TitleScene::ANSWER_PLAYEPISODE_2P)?2:1;
            PlayerState plr;
            plr._chsetup = FileFormats::CreateSavCharacterState();
            plr.characterID=1;
            plr.stateID=1;
            plr._chsetup.id=1;
            plr._chsetup.state=1;
            _game_state.setPlayerState(1, plr);
            plr.characterID=2;
            plr.stateID=1;
            plr._chsetup.id=2;
            plr._chsetup.state=1;
            _game_state.setPlayerState(2, plr);
            _game_state.isEpisode=true;
            episode = res_episode;
            _game_state._episodePath = QFileInfo(episode.worldfile).absoluteDir().absolutePath()+"/";
            _game_state.saveFileName = episode.savefile;
            _game_state.load();
            goto PlayWorldMap;
        default:
            goto PlayWorldMap;
    }

    goto PlayLevel;
}


PlayWorldMap:
{
    int ExitCode=0;

    WorldScene *wScene;
    wScene = new WorldScene();
    bool sceneResult=true;

    if(episode.worldfile.isEmpty())
    {
        sceneResult = false;
        PGE_MsgBox::warn(QObject::tr("No opened files"));
    }
    else
    {
        sceneResult = wScene->loadFile(episode.worldfile);
        wScene->setGameState(&_game_state); //Load game state to the world map
        if(!sceneResult)
        {
            SDL_Delay(50);
            PGE_MsgBox::error(QObject::tr("ERROR:\nFail to start world map\n\n"
                                            "%1")
                              .arg(wScene->getLastError()));
        }
    }

    if(sceneResult)
        sceneResult = wScene->init();

    if(sceneResult)
        wScene->fader.setFade(10, 0.0f, 0.02f);

    if(sceneResult)
        ExitCode = wScene->exec();

    _game_state._recent_ExitCode_world=ExitCode;

    if(wScene->doShutDown())
    {
        delete wScene;
        goto ExitFromApplication;
    }

    if(g_AppSettings.debugMode)
    {
        if(ExitCode==WldExit::EXIT_beginLevel)
        {
            PGE_MsgBox::warn(QObject::tr("Start level\n%1")
                          .arg(_game_state.LevelFile) );
            delete wScene;
            if(_game_state.isHubLevel) goto ExitFromApplication;

            goto PlayWorldMap;
        }
        else
        {
            delete wScene;
            goto ExitFromApplication;
        }
    }

    delete wScene;

    switch(ExitCode)
    {
        case WldExit::EXIT_beginLevel:
            goto PlayLevel;
            break;
        case WldExit::EXIT_close:
            break;
        case WldExit::EXIT_error:
            break;
        case WldExit::EXIT_exitNoSave:
            break;
        case WldExit::EXIT_exitWithSave:
            break;
    }

    if(_flags.testWorld)
        goto ExitFromApplication;

    goto MainMenu;
}


PlayLevel:
{
    bool playAgain = true;
    int entranceID = 0;
    LevelScene* lScene;
    while(playAgain)
    {
        entranceID = _game_state.LevelTargetWarp;

        if(_game_state.LevelFile_hub==_game_state.LevelFile)
        {
                _game_state.isHubLevel=true;
                entranceID = _game_state.game_state.last_hub_warp;
        }

        int ExitCode=0;
            lScene = new LevelScene();

            lScene->setGameState(&_game_state);
            bool sceneResult=true;

            if(_game_state.LevelFile.isEmpty())
            {
                if(g_AppSettings.interprocessing && IntProc::isEnabled())
                {
                    sceneResult = lScene->loadFileIP();
                    if((!sceneResult) && (!lScene->isExiting()))
                    {
                        SDL_Delay(50);
                        PGE_MsgBox msgBox(NULL, QString("ERROR:\nFail to start level\n\n%1")
                                          .arg(lScene->getLastError()),
                                          PGE_MsgBox::msg_error);
                        msgBox.exec();
                    }
                }
                else
                {
                    sceneResult = false;
                    PGE_MsgBox msgBox(NULL, QString("No opened files"),
                                      PGE_MsgBox::msg_warn);
                    msgBox.exec();
                }
            }
            else
            {
                sceneResult = lScene->loadFile(_game_state.LevelFile);
                if(!sceneResult)
                {
                    SDL_Delay(50);
                    PGE_MsgBox msgBox(NULL, QString("ERROR:\nFail to start level\n\n"
                                                    "%1")
                                      .arg(lScene->getLastError()),
                                      PGE_MsgBox::msg_error);
                    msgBox.exec();
                }
            }

            if(sceneResult)
                sceneResult = lScene->setEntrance(entranceID);

            if(sceneResult)
                sceneResult = lScene->init();

            if(sceneResult)
            {
                lScene->fader.setFade(10, 0.0f, 0.02f);
                ExitCode = lScene->exec();
                _game_state._recent_ExitCode_level=ExitCode;
            }

            if(!sceneResult)
                ExitCode = LvlExit::EXIT_Error;

            switch(ExitCode)
            {
            case LvlExit::EXIT_Warp:
                {
                   if(lScene->warpToWorld)
                   {
                       _game_state.game_state.worldPosX = lScene->toWorldXY().x();
                       _game_state.game_state.worldPosY = lScene->toWorldXY().y();
                       _game_state.LevelFile.clear();
                       entranceID = 0;
                       end_level_jump = _game_state.isEpisode ? RETURN_TO_WORLDMAP : RETURN_TO_MAIN_MENU;
                   }
                   else
                   {
                       _game_state.LevelFile = lScene->toAnotherLevel();
                       _game_state.LevelTargetWarp = lScene->toAnotherEntrance();
                       entranceID = _game_state.LevelTargetWarp;

                       if(_game_state.isHubLevel)
                       {
                           _game_state.isHubLevel=false;
                           _game_state.game_state.last_hub_warp = lScene->lastWarpID;
                       }
                   }

                   if(_game_state.LevelFile.isEmpty()) playAgain = false;


                   if(g_AppSettings.debugMode)
                   {
                       if(!fileToOpen.isEmpty())
                       {
                           PGE_MsgBox::warn(QObject::tr("Warp exit\n\nExit to:\n%1\n\nEnter to: %2")
                                         .arg(fileToOpen).arg(entranceID));
                       }
                       playAgain = false;
                   }
                }
                break;
            case LvlExit::EXIT_Closed:
                {
                    end_level_jump=RETURN_TO_EXIT;
                    playAgain = false;
                }
                break;
            case LvlExit::EXIT_MenuExit:
                {
                    end_level_jump = _game_state.isEpisode ? RETURN_TO_WORLDMAP : RETURN_TO_MAIN_MENU;
                    if(_game_state.isHubLevel)
                        end_level_jump = _flags.testLevel ? RETURN_TO_EXIT : RETURN_TO_MAIN_MENU;

                    playAgain = false;
                }
                break;
            case LvlExit::EXIT_PlayerDeath:
                {
                    playAgain = _game_state.isEpisode ? _game_state.replay_on_fail : true;
                    end_level_jump = _game_state.isEpisode ? RETURN_TO_WORLDMAP : RETURN_TO_MAIN_MENU;
                    //check the number of player lives here and decided to return worldmap or gameover
                    if(_game_state.isEpisode)
                    {
                        _game_state.game_state.lives--;
                        if(_game_state.game_state.lives<0)
                        {
                            playAgain=false;
                            _game_state.game_state.coins=0;
                            _game_state.game_state.points=0;
                            _game_state.game_state.lives=3;
                            end_level_jump=RETURN_TO_GAMEOVER_SCREEN;
                        }
                    }
                }
                break;
            case LvlExit::EXIT_Error:
                {
                    end_level_jump = (_game_state.isEpisode)? RETURN_TO_WORLDMAP : RETURN_TO_MAIN_MENU;
                    playAgain = false;
                    PGE_MsgBox::error(QObject::tr("Level was closed with error.\n%1").arg(lScene->errorString()));
                }
                break;
            default:
                end_level_jump = _game_state.isEpisode ? RETURN_TO_WORLDMAP : RETURN_TO_MAIN_MENU;
                playAgain = false;
            }

            if(_flags.testLevel || g_AppSettings.debugMode)
                end_level_jump=RETURN_TO_EXIT;

            ConfigManager::unloadLevelConfigs();
            delete lScene;

            if(g_AppSettings.interprocessing)
                goto ExitFromApplication;
    }

    switch(end_level_jump)
    {
        case RETURN_TO_WORLDMAP:
            goto PlayWorldMap;
        case RETURN_TO_MAIN_MENU:
            goto MainMenu;
        case RETURN_TO_EXIT:
            goto ExitFromApplication;
        case RETURN_TO_GAMEOVER_SCREEN:
            goto GameOverScreen;
        case RETURN_TO_CREDITS_SCREEN:
            goto CreditsScreen;
    }
}
ExitFromApplication:
    ConfigManager::unluadAll();
    if(IntProc::isEnabled()) IntProc::editor->shut();
    PGE_MusPlayer::freeStream();
    PGE_Sounds::clearSoundBuffer();
    Mix_CloseAudio();
    g_AppSettings.save();
    g_AppSettings.closeJoysticks();
    IntProc::quit();
    FontManager::quit();
    PGE_Window::uninit();
    qDebug()<<"<Application closed>";
    a.quit();
    CloseLog();

    return 0;
}

