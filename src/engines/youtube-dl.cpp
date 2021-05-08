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

#include "youtube-dl.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "../networkAccess.h"
#include "../utility.h"

static QJsonObject _defaultControlStructure()
{
	QJsonObject obj ;

	obj.insert( "Connector","&&" ) ;

	obj.insert( "lhs",[](){

		QJsonObject obj ;

		obj.insert( "startsWith","[download]" ) ;

		return obj ;
	}() ) ;

	obj.insert( "rhs",[](){

		QJsonObject obj ;

		obj.insert( "contains","ETA" ) ;

		return obj ;
	}() ) ;

	return obj ;
}

void youtube_dl::init( Logger& logger,const engines::enginePaths& enginePath )
{
	auto m = enginePath.configPath( "youtube-dl.json" ) ;

	if( !QFile::exists( m ) ){

		QJsonObject mainObj ;

		mainObj.insert( "CommandName","youtube-dl" ) ;

		mainObj.insert( "CommandNameWindows","youtube-dl.exe" ) ;

		mainObj.insert( "Name","youtube-dl" ) ;

		mainObj.insert( "DefaultDownLoadCmdOptions",[](){

			QJsonArray arr ;

			arr.append( "--newline" ) ;
			arr.append( "--ignore-config" ) ;
			arr.append( "--no-playlist" ) ;

			return arr ;
		}() ) ;

		mainObj.insert( "DefaultListCmdOptions",[](){

			QJsonArray arr ;
			arr.append( "-F" ) ;
			return arr ;
		}() ) ;

		mainObj.insert( "SkipLineWithText",[](){

			QJsonArray arr ;
			arr.append( "(pass -k to keep)" ) ;
			return arr ;
		}() ) ;

		mainObj.insert( "RemoveText",[](){

			QJsonArray arr ;

			return arr ;
		}() ) ;

		mainObj.insert( "SplitLinesBy",[](){

			QJsonArray arr ;

			arr.append( "\n" ) ;

			return arr ;
		}() ) ;

		mainObj.insert( "PlayListIdArguments",[](){

			QJsonArray arr ;

			arr.append( "--get-id" ) ;
			arr.append( "--get-title" ) ;

			return arr ;
		}() ) ;

		mainObj.insert( "PlayListUrlPrefix","https://youtube.com/watch?v=" ) ;

		mainObj.insert( "PlaylistItemsArgument","--playlist-items" ) ;

		mainObj.insert( "ControlJsonStructure",_defaultControlStructure() ) ;

		mainObj.insert( "DownloadUrl","https://api.github.com/repos/ytdl-org/youtube-dl/releases/latest" ) ;

		mainObj.insert( "VersionArgument","--version" ) ;

		mainObj.insert( "OptionsArgument","-f" ) ;

		mainObj.insert( "BackendPath","${default}" ) ;

		mainObj.insert( "VersionStringLine",0 ) ;

		mainObj.insert( "VersionStringPosition",0 ) ;

		mainObj.insert( "BatchFileArgument","-a" ) ;

		mainObj.insert( "CanDownloadPlaylist",true ) ;

		mainObj.insert( "LikeYoutubeDl",true ) ;

		mainObj.insert( "ReplaceOutputWithProgressReport",false ) ;

		engines::file( m,logger ).write( mainObj ) ;
	}
}

youtube_dl::youtube_dl()
{
}

youtube_dl::~youtube_dl()
{
}

void youtube_dl::updateOptions( QJsonObject& object,settings& settings )
{
	if( !object.contains( "SkipLineWithText" ) ){

		object.insert( "SkipLineWithText",[](){

			QJsonArray arr ;

			arr.append( "(pass -k to keep)" ) ;

			return arr ;
		}() ) ;
	}

	if( !object.contains( "PlayListIdArguments" ) ){

		object.insert( "PlayListIdArguments",[](){

			QJsonArray arr ;

			arr.append( "--get-id" ) ;
			arr.append( "--get-title" ) ;

			return arr ;
		}() ) ;
	}

	if( !object.contains( "ControlJsonStructure" ) ){

		object.insert( "ControlJsonStructure",_defaultControlStructure() ) ;
	}

	if( !object.contains( "PlayListIdArgument" ) ){

		object.insert( "PlayListIdArgument","--get-id" ) ;
	}

	if( !object.contains( "PlaylistItemsArgument" ) ){

		object.insert( "PlaylistItemsArgument","--playlist-items" ) ;
	}

	if( !object.contains( "PlayListUrlPrefix" ) ){

		object.insert( "PlayListUrlPrefix","https://youtube.com/watch?v=" ) ;
	}

	object.insert( "UsePrivateExecutable",!settings.useSystemProvidedVersionIfAvailable() ) ;
}

std::unique_ptr< engines::engine::functions::filter > youtube_dl::Filter()
{
	return std::make_unique< youtube_dl::youtube_dlFilter >() ;
}

void youtube_dl::updateDownLoadCmdOptions( const engines::engine& engine,
					   const QString& quality,
					   const QStringList& userOptions,
					   QStringList& urls,
					   QStringList& ourOptions )
{
	Q_UNUSED( urls )

	if( userOptions.contains( "--yes-playlist" ) ){

		ourOptions.removeAll( "--no-playlist" ) ;
	}

	ourOptions.append( engine.optionsArgument() ) ;

	if( quality.isEmpty() ){

		ourOptions.append( "best" ) ;
	}else{
		ourOptions.append( quality ) ;
	}
}

youtube_dl::youtube_dlFilter::youtube_dlFilter() :
	m_counter( 0 ),
	m_processing( QObject::tr( "Processing" ) ),
	m_downloadCompleted( QObject::tr( "Download completed" ) )
{
}

const QString& youtube_dl::youtube_dlFilter::operator()( const engines::engine&,
							 const Logger::Data& s )
{
	const auto a = utility::split( s.toString(),'\n',true ) ;

	auto m = utility::make_reverseIterator( a ) ;
	/*
	 * Going the first time looking for a file name
	 */
	while( m.hasNext() ){

		const auto& e = m.next() ;

		if( e.startsWith( "[download] " ) && e.contains( " has already been downloaded" ) ){

			m_tmp = e.mid( e.indexOf( " " ) ) ;
			m_tmp.truncate( m_tmp.indexOf( " has already been downloaded" ) ) ;
			m_tmp += "\n" + m_downloadCompleted ;

			m_final = m_tmp ;

			return m_tmp ;

		}else if( e.contains( " Merging formats into \"" ) ){

			m_tmp = e.mid( e.indexOf( "\"" ) + 1 ) ;
			m_tmp.truncate( m_tmp.size() - 1 ) ;
			m_tmp += "\n" + m_downloadCompleted ;

			m_final = m_tmp ;

			return m_tmp ;

		}else if( e.contains( "] Destination: " ) ){

			m_name = e.mid( e.indexOf( "] Destination: " ) + 15 ) ;
			break ;

		}else if( e.startsWith( "ERROR: " ) ){

			m_tmp = e ;
			return m_tmp ;
		}
	}

	/*
	 * Going the second time looking for progress report
	 */
	m.reset() ;

	while( m.hasNext() ){

		const auto& e = m.next() ;

		if( e.startsWith( "[download] 100% of " ) ){

			m_tmp = m_name + "\n" + m_downloadCompleted ;

			return m_tmp ;

		}else if( e.startsWith( "[download]  " ) && e.contains( " ETA " ) ){

			m_tmp = e ;
			m_tmp.replace( "[download]  ","" ) ;

			if( m_name.isEmpty() ){

				qDebug() << "Failed to find downloading file name" ;
			}else{
				m_tmp = m_name + "\n" + m_tmp ;
			}

			return m_tmp ;
		}
	}

	if( m_final.isEmpty() ){

		return this->processing() ;
	}else{
		return m_final ;
	}
}

youtube_dl::youtube_dlFilter::~youtube_dlFilter()
{
}

const QString& youtube_dl::youtube_dlFilter::processing()
{
	if( m_counter < 8 ){

		m_processing += " ..." ;
	}else{
		m_counter = 0 ;
		m_processing = QObject::tr( "Processing" ) + " ..." ;
	}

	m_counter++ ;

	return m_processing ;
}
