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

#include "batchdownloader.h"
#include "tabmanager.h"
#include "mainwindow.h"

#include <QMetaObject>
#include <QClipboard>
#include <QFileDialog>

batchdownloader::batchdownloader( const Context& ctx ) :
	m_ctx( ctx ),
	m_settings( m_ctx.Settings() ),
	m_ui( m_ctx.Ui() ),
	m_mainWindow( m_ctx.mainWidget() ),
	m_tabManager( m_ctx.TabManager() ),
	m_showThumbnails( m_settings.showThumbnails() ),
	m_table( *m_ui.tableWidgetBD,m_ctx.mainWidget().font(),1 ),
	m_tableWidgetBDList( *m_ui.TableWidgetBatchDownloaderList,m_ctx.mainWidget().font(),0 ),
	m_debug( ctx.debug() ),
	m_defaultVideoThumbnail( m_settings.defaultVideoThumbnailIcon( settings::tabName::batch ) ),
	m_ccmd( m_ctx,*m_ui.pbBDCancel,m_settings )
{
	qRegisterMetaType< ItemEntry >() ;

	m_tableWidgetBDList.setTableWidget( [](){

		tableWidget::tableWidgetOptions opts ;

		opts.customContextPolicy = Qt::NoContextMenu ;
		opts.selectionMode       = QAbstractItemView::ExtendedSelection ;

		return opts ;
	}() ) ;

	m_table.connect( &QTableWidget::currentItemChanged,[ this ]( QTableWidgetItem * c,QTableWidgetItem * p ){

		m_table.selectRow( c,p,m_table.startPosition() ) ;
	} ) ;

	m_table.hideColumns( 2,3,4 ) ;

	this->setThumbnailColumnSize( m_showThumbnails ) ;

	m_ui.pbBDDownload->setEnabled( false ) ;

	m_ui.pbBDCancel->setEnabled( false ) ;

	m_ui.BDFrame->hide() ;

	this->resetMenu() ;

	m_tableWidgetBDList.connect( &QTableWidget::itemClicked,[ this ]( QTableWidgetItem * item ){

		if( item ){

			m_tableWidgetBDList.selectMediaOptions( m_optionsList,*item,m_lineEdit ) ;
		}
	} ) ;

	m_tableWidgetBDList.connect( &QTableWidget::itemDoubleClicked,[ this ]( QTableWidgetItem * item ){

		if( item ){

			auto m = m_tableWidgetBDList.item( item->row(),0 ).text() ;

			if( !m.isEmpty() ){

				auto u = tableWidget::type::DownloadOptions ;

				m_table.setDownloadingOptions( u,m_table.currentRow(),m ) ;
			}

			m_ui.BDFrame->hide() ;
		}
	} ) ;

	connect( m_ui.pbBatchDownloaderSet,&QPushButton::clicked,[ this ](){

		auto m = m_lineEdit.text() ;

		if( !m.isEmpty() ){

			auto u = tableWidget::type::DownloadOptions ;

			m_table.setDownloadingOptions( u,m_table.currentRow(),m ) ;
		}

		m_ui.BDFrame->hide() ;
	} ) ;

	connect( m_ui.pbCancelBatchDownloder,&QPushButton::clicked,[ this ](){

		m_ui.BDFrame->hide() ;
	} ) ;

	connect( m_ui.pbBDOptionsHistory,&QPushButton::clicked,[ this ](){

		auto s = utility::showHistory( *m_ui.lineEditBDUrlOptions,
					       m_settings.getOptionsHistory( settings::tabName::batch ),
					       m_settings,settings::tabName::batch ) ;

		if( s ){

			this->download( this->defaultEngine() ) ;
		}
	} ) ;

	connect( m_ui.pbBDDownload,&QPushButton::clicked,[ this ](){

		this->download( this->defaultEngine() ) ;
	} ) ;

	connect( m_ui.pbBDCancel,&QPushButton::clicked,[ this ](){

		m_terminator.terminateAll( m_table.get() ) ;
	} ) ;

	connect( m_ui.pbBDPasteClipboard,&QPushButton::clicked,[ this ](){

		auto m = utility::clipboardText() ;

		if( !m.isEmpty() ){

			this->addToList( m ) ;
		}
	} ) ;

	m_table.connect( &QTableWidget::cellDoubleClicked,[ this ]( int row,int column ){

		Q_UNUSED( column )

		m_ctx.Engines().openUrls( m_table,
					  row,
					  m_ui.cbEngineTypeBD->currentText() ) ;
	} ) ;

	auto s = static_cast< void( QComboBox::* )( int ) >( &QComboBox::activated ) ;

	connect( m_ui.cbEngineTypeBD,s,[ & ]( int s ){

		if( s != -1 ){

			auto m = m_ui.cbEngineTypeBD->itemText( s ) ;

			m_ui.lineEditBDUrlOptions->setText( m_settings.lastUsedOption( m,settings::tabName::batch ) ) ;

			m_settings.setDefaultEngine( m,settings::tabName::batch ) ;
		}
	} ) ;

	m_table.connect( &QTableWidget::customContextMenuRequested,[ this ]( QPoint ){

		auto row = m_table.currentRow() ;

		auto function = [ this ]( const utility::contextState& c ){

			if( c.showLogWindow() ){

				m_ctx.logger().showLogWindow() ;

			}else if( c.clear() ){

				m_table.clear() ;
			}
		} ;

		if( row == -1 ){

			QMenu m ;

			this->getListFromFile( m ) ;

			return utility::appendContextMenu( m,m_table.noneAreRunning(),std::move( function ) ) ;
		}

		auto txt = m_table.runningState( row ) ;

		auto running = downloadManager::finishedStatus::running( txt ) ;
		auto finishSuccess = downloadManager::finishedStatus::finishedWithSuccess( txt ) ;

		QMenu m ;

		auto ac = m.addAction( tr( "Open" ) ) ;

		ac->setEnabled( finishSuccess ) ;

		connect( ac,&QAction::triggered,[ this,row ](){

			auto m = m_ui.cbEngineTypeBD->currentText() ;
			m_ctx.Engines().openUrls( m_table,row,m ) ;
		} ) ;

		ac = m.addAction( tr( "Cancel" ) ) ;

		ac->setEnabled( running ) ;

		connect( ac,&QAction::triggered,[ this,row ](){

			m_terminator.terminate( row ) ;
		} ) ;

		ac = m.addAction( tr( "Copy Url" ) ) ;

		connect( ac,&QAction::triggered,[ this,row ](){

			auto m = QApplication::clipboard() ;

			if( m ){

				m->setText( m_table.url( row ) ) ;
			}
		} ) ;

		ac = m.addAction( tr( "Remove" ) ) ;

		ac->setEnabled( m_table.noneAreRunning() ) ;

		connect( ac,&QAction::triggered,[ this,row ](){

			m_table.removeRow( row ) ;

			m_ui.pbBDDownload->setEnabled( m_table.rowCount() ) ;
		} ) ;

		const auto& engine = this->defaultEngine() ;

		ac = m.addAction( tr( "Show Media Options" ) ) ;
		ac->setEnabled( !running && engine.likeYoutubeDl() ) ;

		connect( ac,&QAction::triggered,[ this ](){

			this->showList() ;
		} ) ;		

		utility::addDownloadContextMenu( running,finishSuccess,m,row,[ this ]( int row ){

			auto m = m_table.uiText( row ) ;

			return m.endsWith( "\n" + engines::engine::mediaAlreadInArchiveText() ) ;

		},[ this,&engine ]( QAction * ac,bool forceDownload,int row ){

			connect( ac,&QAction::triggered,[ &engine,this,row,forceDownload ](){

				downloadManager::index indexes( m_table ) ;

				auto e = m_table.runningState( row ) ;

				if( !downloadManager::finishedStatus::finishedWithSuccess( e ) || forceDownload ){

					auto u = m_table.downloadingOptions( row ) ;

					if( u.isEmpty() ){

						indexes.add( row,m_ui.lineEditBDUrlOptions->text(),forceDownload ) ;
					}else{
						indexes.add( row,u,forceDownload ) ;
					}
				}

				this->download( engine,std::move( indexes ) ) ;
			} ) ;
		} ) ;

		this->getListFromFile( m ) ;

		utility::saveDownloadList( m_ctx,m,m_table ) ;

		auto mm = m.addMenu( tableWidget::engineName().replace( ":","" ) ) ;

		mm->setEnabled( !finishSuccess ) ;

		for( const auto& it : m_ctx.Engines().getEngines() ){

			if( it.mainEngine() ){

				const auto& e = it.name() ;

				mm->addAction( e )->setObjectName( e ) ;
			}
		}

		connect( mm,&QMenu::triggered,[ this ]( QAction * ac ){

			auto u = tableWidget::type::EngineName ;

			m_table.setDownloadingOptions( u,m_table.currentRow(),ac->objectName() ) ;
		} ) ;

		auto subMenu = utility::setUpMenu( m_ctx,{},false,false,true,&m ) ;

		subMenu->setTitle( QObject::tr( "Preset Options" ) ) ;

		subMenu->setEnabled( !finishSuccess ) ;

		connect( subMenu,&QMenu::triggered,[ this,row ]( QAction * ac ){

			auto m = util::split( ac->objectName(),'\n',true ) ;

			auto u = tableWidget::type::DownloadOptions ;

			if( m.size() > 1 ){

				m_table.setDownloadingOptions( u,row,m[ 0 ],m[ 1 ] ) ;
			}else{
				m_table.setDownloadingOptions( u,row,m[ 0 ] ) ;
			}
		} ) ;

		m.addMenu( subMenu ) ;

		m.addSeparator() ;

		utility::appendContextMenu( m,{ m_table.noneAreRunning(),finishSuccess },std::move( function ) ) ;
	} ) ;

	connect( m_ui.pbBDQuit,&QPushButton::clicked,[ this ](){

		m_tabManager.basicDownloader().appQuit() ;
	} ) ;

	connect( m_ui.pbBDCancel,&QPushButton::clicked,[ this ](){

		m_ccmd.cancelled() ;
	} ) ;

	connect( m_ui.pbBDAdd,&QPushButton::clicked,[ this ](){

		auto m = m_ui.lineEditBDUrl->text() ;

		if( !m.isEmpty() ){

			m_ui.lineEditBDUrl->clear() ;

			this->addToList( m ) ;
		}
	} ) ;
}

void batchdownloader::init_done()
{
}

void batchdownloader::resetMenu()
{
	utility::setMenuOptions( m_ctx,{},false,true,m_ui.pbBDOptions,[ this ]( QAction * aa ){

		utility::selectedAction ac( aa ) ;

		if( ac.clearOptions() ){

			m_ui.lineEditBDUrlOptions->clear() ;

		}else if( ac.clearScreen() ){

			this->clearScreen() ;

		}else if( ac.openFolderPath() ){

			utility::openDownloadFolderPath( m_settings.downloadFolder() ) ;
		}else{
			m_ui.lineEditBDUrlOptions->setText( ac.objectName() ) ;

			this->download( this->defaultEngine() ) ;
		}
	} ) ;
}

void batchdownloader::retranslateUi()
{
	this->resetMenu() ;
}

void batchdownloader::tabEntered()
{
	auto m = m_ui.cbEngineTypeBD->currentText() ;

	m_ui.lineEditBDUrlOptions->setText( m_settings.lastUsedOption( m,settings::tabName::batch ) ) ;
	m_ui.lineEditBDUrl->setFocus() ;
}

void batchdownloader::tabExited()
{
}

void batchdownloader::gotEvent( const QString& m )
{
	if( !m.isEmpty() ){

		m_ui.tabWidget->setCurrentIndex( 1 ) ;

		this->addToList( m ) ;
	}
}

void batchdownloader::updateEnginesList( const QStringList& e )
{
	auto& comboBox = *m_ui.cbEngineTypeBD ;

	comboBox.clear() ;

	for( const auto& it : e ){

		comboBox.addItem( it ) ;
	}

	auto s = settings::tabName::batch ;

	this->setUpdefaultEngine( comboBox,
				  this->defaultEngineName(),
				  [ this,s ]( const QString& e ){ m_settings.setDefaultEngine( e,s ) ; } ) ;
}

void batchdownloader::showThumbnail( const engines::engine& engine,const QStringList& list )
{
	if( m_showThumbnails && engine.likeYoutubeDl() ){

		for( const auto& it : list ){

			downloadManager::index indexes( m_table ) ;

			auto state = downloadManager::finishedStatus::running() ;

			int row = m_table.addItem( m_defaultVideoThumbnail,{ "...\n" + it,it,state } ) ;

			util::Timer( 1000,[ this,row ]( int counter ){

				if( downloadManager::finishedStatus::running( m_table.runningState( row ) ) ){

					QString m = "..." ;

					int max = counter % 4 ;

					for( int s = 0 ; s < max ; s++ ){

						m += " ..." ;
					}

					m_table.uiTextItem( row ).setText( m + "\n" + m_table.url( row ) ) ;

					return false ;
				}else{
					return true ;
				}
			} ) ;

			m_table.selectLast() ;

			indexes.add( row,m_ui.lineEditBDUrlOptions->text() ) ;

			m_ccmd.download( std::move( indexes ),engine,[ this ](){

				return m_settings.maxConcurrentDownloads() ;

			}(),[ this,it ]( const engines::engine& engine,int index ){

				this->showThumbnail( engine,index,it,false ) ;
			} ) ;
		}
	}else{
		this->addItemUiSlot( { engine,list } ) ;
	}
}

void batchdownloader::addItemUiSlot( ItemEntry m )
{
	if( m.hasNext() ){

		this->addItemUi( m_defaultVideoThumbnail,-1,false,m.next() ) ;

		QMetaObject::invokeMethod( this,"addItemUiSlot",Qt::QueuedConnection,Q_ARG( ItemEntry,m ) ) ;
	}
}

void batchdownloader::getListFromFile( QMenu& m )
{
	auto ac = m.addAction( QObject::tr( "Get List From File" ) ) ;

	QObject::connect( ac,&QAction::triggered,[ this ](){

		auto e = QFileDialog::getOpenFileName( &m_ctx.mainWindow(),tr( "Set Batch File" ),QDir::homePath() ) ;

		if( !e.isEmpty() ){

			auto list = engines::file( e,m_ctx.logger() ).readAllAsLines() ;

			if( !list.isEmpty() ){

				const auto& engine = this->defaultEngine() ;

				this->showThumbnail( engine,list ) ;
			}
		}
	} ) ;
}

QString batchdownloader::defaultEngineName()
{
	return m_settings.defaultEngine( settings::tabName::batch,m_ctx.Engines().defaultEngineName() ) ;
}

const engines::engine& batchdownloader::defaultEngine()
{
	return m_ctx.Engines().defaultEngine( this->defaultEngineName() ) ;
}

void batchdownloader::showThumbnail( const engines::engine& engine,int index,const QString& url,bool autoDownload )
{
	auto aa = [ &engine,index,this,url,autoDownload ]( utility::ProcessExitState e,const batchdownloader::opts& opts ){

		auto aa = [ this,autoDownload ]( const engines::engine& engine,int index ){

			this->showThumbnail( engine,index,{},autoDownload ) ;
		} ;

		auto bb = [ this,&engine,&opts,url,autoDownload ]( const downloadManager::finishedStatus& f ){

			auto allFinished = f.allFinished() ;

			if( f.exitState().cancelled() ){

				this->addItem( f.index(),allFinished,url ) ;
			}else{
				utility::MediaEntry m( opts.batchLogger.data() ) ;

				if( m.valid() ){

					this->addItem( f.index(),allFinished,std::move( m ) ) ;
				}else{
					this->addItem( f.index(),allFinished,url ) ;
				}

				if( allFinished && autoDownload ){

					this->download( engine ) ;
				}
			}
		} ;

		m_ccmd.monitorForFinished( engine,index,std::move( e ),std::move( aa ),std::move( bb ) ) ;
	} ;

	auto functions = utility::OptionsFunctions( [ this ]( const batchdownloader::opts& ){

		m_ui.pbBDPasteClipboard->setEnabled( true ) ;

		m_ui.pbBDAdd->setEnabled( true ) ;

		m_ui.lineEditBDUrl->setEnabled( true ) ;

	},std::move( aa ) ) ;

	auto m = m_ui.lineEditBDUrlOptions->text() ;

	BatchLoggerWrapper wrapper( m_ctx.logger() ) ;

	m_ccmd.download( engine,
			 engine.dumpJsonArguments(),
			 index == -1 ? url : m_table.url( index ),
			 m_terminator.setUp( m_ui.pbBDCancel,&QPushButton::clicked,index ),
			 batchdownloader::make_options( { m_ctx,m_debug,false,index,wrapper },std::move( functions ) ),
			 wrapper ) ;
}

void batchdownloader::setThumbnailColumnSize( bool e )
{
	m_showThumbnails = e ;

	if( m_showThumbnails ){

		m_table.get().setColumnWidth( 0,m_settings.thumbnailWidth( settings::tabName::batch ) ) ;
	}else{
		auto w = m_settings.thumbnailWidth( settings::tabName::batch ) ;
		auto h = m_settings.thumbnailHeight( settings::tabName::batch ) ;

		auto pixmap = QIcon( ":/video" ).pixmap( w,h ) ;

		m_table.get().setColumnWidth( 0,pixmap.width() ) ;
	}
}

void batchdownloader::clearScreen()
{
	m_table.clear() ;
	m_ui.lineEditBDUrlOptions->clear() ;
	m_ui.lineEditBDUrl->clear() ;
}

void batchdownloader::showList()
{
	auto& table = m_table.get() ;

	auto row = table.currentRow() ;

	if( row == -1 ){

		return ;
	}

	const auto& engine = utility::resolveEngine( m_table.uiText( row ),
						     this->defaultEngine(),
						     m_ctx.Engines() ) ;


	auto args = engine.defaultListCmdOptions() ;

	if( args.isEmpty() ){

		return ;
	}

	auto cookiePath = m_settings.cookieFilePath( engine.name() ) ;
	const auto& ca = engine.cookieArgument() ;

	if( !cookiePath.isEmpty() && !ca.isEmpty() ){

		args.append( ca ) ;
		args.append( cookiePath ) ;
	}

	m_optionsList.clear() ;

	m_tableWidgetBDList.clear() ;

	m_ui.BDFrame->show() ;

	args.append( m_table.url( row ) ) ;

	m_ctx.TabManager().disableAll() ;

	auto functions = utility::OptionsFunctions( [ this,&engine ]( const QList< QByteArray >& args ){

			m_tableWidgetBDList.showOptions( engine,args ) ;

		},[ this ]( const batchdownloader::opts& opts ){

			opts.ctx.TabManager().disableAll() ;

			m_ui.pbCancelBatchDownloder->setEnabled( true ) ;

		},[]( utility::ProcessExitState,const batchdownloader::opts& opts ){

			opts.ctx.TabManager().enableAll() ;
		}
	) ;

	batchdownloader::opts opts{ m_ctx,m_debug,true,-1,BatchLoggerWrapper( m_ctx.logger() ) } ;

	auto oopts  = batchdownloader::make_options( std::move( opts ),std::move( functions ) ) ;
	auto logger = LoggerWrapper( m_ctx.logger(),utility::concurrentID() ) ;
	auto term   = m_terminator.setUp( m_ui.pbCancelBatchDownloder,&QPushButton::clicked,-1 ) ;

	auto ctx    = utility::make_ctx( engine,std::move( oopts ),std::move( logger ),std::move( term ) ) ;

	utility::run( args,QString(),std::move( ctx ) ) ;
}

static void _addItemUi( const QPixmap& pixmap,
			int index,
			tableWidget& table,
			Ui::MainWindow& ui,
			const utility::MediaEntry& media )
{
	auto state = downloadManager::finishedStatus::notStarted() ;

	if( index == -1 ){

		table.addItem( pixmap,{ media.uiText(),media.url(),state },Qt::AlignCenter ) ;
		table.selectLast() ;
	}else{
		table.replace( pixmap,{ media.uiText(),media.url(),state },index ) ;
	}

	ui.lineEditBDUrl->clear() ;

	ui.lineEditBDUrl->setFocus() ;
}

void batchdownloader::addItemUi( const QPixmap& pixmap,
				 int index,
				 bool enableAll,
				 const utility::MediaEntry& media )
{
	_addItemUi( pixmap,index,m_table,m_ui,media ) ;

	m_ui.pbBDDownload->setEnabled( true ) ;

	if( enableAll ){

		m_ctx.TabManager().enableAll() ;
	}
}

void batchdownloader::addItemUi( int index,bool enableAll,const utility::MediaEntry& media )
{
	this->addItemUi( m_defaultVideoThumbnail,index,enableAll,media ) ;
}

void batchdownloader::addItem( int index,bool enableAll,const utility::MediaEntry& media )
{
	if( media.thumbnailUrl().isEmpty() ){

		this->addItemUi( index,enableAll,media ) ;
	}else{
		if( networkAccess::hasNetworkSupport() ){

			auto& network = m_ctx.TabManager().Configure().network() ;

			m_networkRunning++ ;

			network.getResource( media.thumbnailUrl(),
					     [ this,media = media,index ]( const QByteArray& data ){

				QPixmap pixmap ;

				if( pixmap.loadFromData( data ) ){

					auto w = m_settings.thumbnailWidth( settings::tabName::batch ) ;
					auto h = m_settings.thumbnailHeight( settings::tabName::batch ) ;

					pixmap = pixmap.scaled( w,h ) ;
				}else{
					pixmap = m_defaultVideoThumbnail ;
				}

				_addItemUi( pixmap,index,m_table,m_ui,media ) ;

				if( m_table.noneAreRunning() ){

					m_ui.pbBDDownload->setEnabled( true ) ;

					m_ctx.TabManager().enableAll() ;
				}

				m_networkRunning-- ;
			} ) ;
		}else{
			this->addItemUi( index,enableAll,media ) ;
		}
	}
}

void batchdownloader::addToList( const QString& url )
{
	this->showThumbnail( this->defaultEngine(),{ url } ) ;
}

void batchdownloader::download( const engines::engine& engine,downloadManager::index indexes )
{
	if( indexes.empty() ){

		return ;
	}

	m_settings.setLastUsedOption( m_ui.cbEngineTypeBD->currentText(),
				      m_ui.lineEditBDUrlOptions->text(),
				      settings::tabName::batch ) ;

	m_ctx.TabManager().basicDownloader().hideTableList() ;

	m_ctx.mainWindow().setTitle( QString() ) ;

	m_ccmd.download( std::move( indexes ),engine,[ this ](){

		return m_settings.maxConcurrentDownloads() ;

	}(),[ this ]( const engines::engine& engine,int index ){

		this->download( engine,index ) ;
	} ) ;
}

void batchdownloader::download( const engines::engine& engine )
{
	downloadManager::index indexes( m_table ) ;

	for( int s = 0 ; s < m_table.rowCount() ; s++ ){

		auto e = m_table.runningState( s ) ;

		if( !downloadManager::finishedStatus::finishedWithSuccess( e ) ){

			auto u = m_table.downloadingOptions( s ) ;

			if( u.isEmpty() ){

				indexes.add( s,m_ui.lineEditBDUrlOptions->text() ) ;
			}else{
				indexes.add( s,u ) ;
			}
		}
	}

	this->download( engine,std::move( indexes ) ) ;
}

void batchdownloader::download( const engines::engine& eng,int index )
{
	const auto& engine = utility::resolveEngine( m_table.uiText( index ),eng,m_ctx.Engines() ) ;

	auto aa = [ &engine,index,this ]( utility::ProcessExitState e,const batchdownloader::opts& ){

		auto aa = [ this ]( const engines::engine& engine,int index ){

			this->download( engine,index ) ;
		} ;

		auto bb = [ &engine,index,this ]( const downloadManager::finishedStatus& f ){

			utility::updateFinishedState( engine,m_settings,m_table,f ) ;

			if( m_table.noneAreRunning() ){

				m_ctx.TabManager().enableAll() ;
			}

			m_ctx.mainWindow().setTitle( m_table.completeProgress( index ) ) ;
		} ;

		m_ccmd.monitorForFinished( engine,index,std::move( e ),std::move( aa ),std::move( bb ) ) ;
	} ;

	auto functions = utility::OptionsFunctions( []( const batchdownloader::opts& ){},std::move( aa ) ) ;

	auto m = m_ui.lineEditBDUrlOptions->text() ;

	m_settings.addOptionsHistory( m,settings::tabName::batch ) ;

	batchdownloader::opts opts{ m_ctx,m_debug,false,index,BatchLoggerWrapper( m_ctx.logger() ) } ;

	auto oopts = batchdownloader::make_options( std::move( opts ),std::move( functions ) ) ;

	auto logger = make_loggerBatchDownloader( engine.filter( utility::args( m ).quality() ),
						  m_ctx.logger(),
						  m_table.uiTextItem( index ),
						  utility::concurrentID() ) ;

	m_ccmd.download( engine,
			 m_ctx.Engines().engineDirPaths(),
			 m_table.runningStateItem( index ),
			 m_table.url( index ),
			 m_terminator.setUp(),
			 std::move( oopts ),
			 std::move( logger ) ) ;
}

void batchdownloader::enableAll()
{
	m_table.setEnabled( true ) ;
	m_ui.pbBDPasteClipboard->setEnabled( true ) ;
	m_ui.pbBDDownload->setEnabled( m_table.rowCount() ) ;
	m_ui.pbBDAdd->setEnabled( true ) ;
	m_ui.pbBDOptions->setEnabled( true ) ;
	m_ui.labelBDEnterOptions->setEnabled( true ) ;
	m_ui.labelBDEnterUrl->setEnabled( true ) ;
	m_ui.pbBDQuit->setEnabled( true ) ;
	m_ui.lineEditBDUrl->setEnabled( true ) ;
	m_ui.lineEditBDUrlOptions->setEnabled( true ) ;
	m_ui.labelBDEngineName->setEnabled( true ) ;
	m_ui.cbEngineTypeBD->setEnabled( true ) ;
	m_ui.pbBatchDownloaderSet->setEnabled( true ) ;
	m_ui.pbCancelBatchDownloder->setEnabled( true ) ;
	m_ui.TableWidgetBatchDownloaderList->setEnabled( true ) ;
	m_ui.pbBDOptionsHistory->setEnabled( true ) ;
}

void batchdownloader::disableAll()
{
	m_table.setEnabled( false ) ;
	m_ui.pbBDOptionsHistory->setEnabled( false ) ;
	m_ui.pbBDPasteClipboard->setEnabled( false ) ;
	m_ui.cbEngineTypeBD->setEnabled( false ) ;
	m_ui.labelBDEngineName->setEnabled( false ) ;
	m_ui.pbBDDownload->setEnabled( false ) ;
	m_ui.pbBDAdd->setEnabled( false ) ;
	m_ui.pbBDOptions->setEnabled( false ) ;
	m_ui.labelBDEnterOptions->setEnabled( false ) ;
	m_ui.labelBDEnterUrl->setEnabled( false ) ;
	m_ui.pbBDQuit->setEnabled( false ) ;
	m_ui.lineEditBDUrl->setEnabled( false ) ;
	m_ui.lineEditBDUrlOptions->setEnabled( false ) ;
	m_ui.pbBatchDownloaderSet->setEnabled( false ) ;
	m_ui.pbCancelBatchDownloder->setEnabled( false ) ;
	m_ui.TableWidgetBatchDownloaderList->setEnabled( false ) ;
}
