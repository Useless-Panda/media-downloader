/*
 *
 *  Copyright (c) 2021
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ARGS_H
#define ARGS_H

class translator ;
class QMainWindow ;
class tabManager ;
class settings ;
class MainWindow ;
class MainWindowUi ;
class Logger ;
class engines ;

#include "networkAccess.h"
#include "versionInfo.h"

class QWidget ;

namespace utility{
	class downloadDefaultOptions ;
}

#include<QString>

namespace Ui
{
	class MainWindow ;
}

class Context{
public:
	Context( settings& s,
		 translator& t,
		 Ui::MainWindow& m,
		 QWidget& w,
		 MainWindow& mw,
		 Logger& l,
		 engines& e,
		 tabManager& tm,
		 QString debug ) :
		m_settings( s ),
		m_translator( t ),
		m_mainUi( m ),
		m_mainWidget( w ),
		m_mainWindow( mw ),
		m_logger( l ),
		m_engines( e ),
		m_tabManager( tm ),
		m_debug( std::move( debug ) ),
		m_networkAccess( *this ),
		m_versionInfo( m,*this )
	{
	}
	const versionInfo& getVersionInfo() const
	{
		return m_versionInfo ;
	}
	const networkAccess& network() const
	{
		return m_networkAccess ;
	}
	engines& Engines() const
	{
		return m_engines ;
	}
	settings& Settings() const
	{
		return m_settings ;
	}
	translator& Translator() const
	{
		 return m_translator ;
	}
	Ui::MainWindow& Ui() const
	{
		return m_mainUi ;
	}
	QWidget& mainWidget() const
	{
		return m_mainWidget ;
	}
	MainWindow& mainWindow() const
	{
		return m_mainWindow ;
	}
	tabManager& TabManager() const
	{
		return m_tabManager ;
	}
	Logger& logger() const
	{
		return m_logger ;
	}
	const QString& debug() const
	{
		return m_debug ;
	}
private:
	settings& m_settings ;
	translator& m_translator ;
	Ui::MainWindow& m_mainUi ;
	QWidget& m_mainWidget ;
	MainWindow& m_mainWindow ;
	Logger& m_logger ;
	engines& m_engines ;
	tabManager& m_tabManager ;
	QString m_debug ;
	networkAccess m_networkAccess ;
	versionInfo m_versionInfo ;
};

#endif
