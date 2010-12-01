//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: header.cpp,v 1.1.1.1 2003/10/27 18:55:05 wschweer Exp $
//  (C) Copyright 2000 Werner Schweer (ws@seh.de)
//=========================================================

#include "header.h"
#include "xml.h"
#include <qstringlist.h>


#include <QStandardItemModel>

//---------------------------------------------------------
//   readStatus
//---------------------------------------------------------

void Header::readStatus(Xml& xml)
      {
      for (;;) {
            Xml::Token token = xml.parse();
            const QString& tag = xml.s1();
            switch (token) {
                  case Xml::Error:
                  case Xml::End:
                        return;
                  case Xml::Text:
                        {
                        QStringList l = QStringList::split(QString(" "), tag);
                        int index = count();
                        for (QStringList::Iterator it = l.begin(); it != l.end(); ++it) {
                              int section = (*it).toInt();
                              moveSection(section, index);
                              --index;
                              }
                        }
                        break;
                  case Xml::TagStart:
                        xml.unknown("Header");
                        break;
                  case Xml::TagEnd:
                        if (tag == name())
                              return;
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void Header::writeStatus(int level, Xml& xml) const
      {
      //xml.nput(level, "<%s> ", name());
      xml.nput(level, "<%s> ", Xml::xmlString(name()).latin1());
      int n = count() - 1;
      for (int i = n; i >= 0; --i)
            xml.nput("%d ", mapToSection(i));
      //xml.put("</%s>", name());
      xml.put("</%s>", Xml::xmlString(name()).latin1());
      }



//---------------------------------------------------------
//   readStatus
//---------------------------------------------------------

void HeaderNew::readStatus(Xml& xml)
      {
      for (;;) {
            Xml::Token token = xml.parse();
            const QString& tag = xml.s1();
            switch (token) {
                  case Xml::Error:
                  case Xml::End:
                        return;
                  case Xml::Text:
                        {
                        QStringList l = QStringList::split(QString(" "), tag);
                        int index = count();
                        for (QStringList::Iterator it = l.begin(); it != l.end(); ++it) {
                              int section = (*it).toInt();
                              moveSection(section, index);
                              --index;
                              }
                        }
                        break;
                  case Xml::TagStart:
                        xml.unknown("Header");
                        break;
                  case Xml::TagEnd:
                        if (tag == name())
                              return;
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void HeaderNew::writeStatus(int level, Xml& xml) const
      {
      //xml.nput(level, "<%s> ", name());
      xml.nput(level, "<%s> ", Xml::xmlString(name()).latin1());
      int n = count() - 1;
      for (int i = n; i >= 0; --i)
            xml.nput("%d ", visualIndex(i));
      //xml.put("</%s>", name());
      xml.put("</%s>", Xml::xmlString(name()).latin1());
      }

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

HeaderNew::HeaderNew(QWidget* parent, const char* name)
  : QHeaderView(Qt::Horizontal, parent) 
      {
      setObjectName(name);
      itemModel = new QStandardItemModel;
      setModel(itemModel);
      columncount = 0;
      //setResizeMode(QHeaderView::ResizeToContents);
      setDefaultSectionSize(30);
      }

//---------------------------------------------------------
//   addLabel
//---------------------------------------------------------

int HeaderNew::addLabel(const QString & text, int size )
      {
      QStandardItem *sitem = new QStandardItem(text );
      itemModel->setHorizontalHeaderItem(columncount, sitem);
      if (size > -1)
            resizeSection(columncount, size);

      return columncount++;
      }

//---------------------------------------------------------
//   setToolTip
//---------------------------------------------------------

void HeaderNew::setToolTip(int col, const QString &text)
      {
      QStandardItem *item = itemModel->horizontalHeaderItem(col);
      item->setToolTip(text);
      }

//---------------------------------------------------------
//   setWhatsThis
//---------------------------------------------------------

void HeaderNew::setWhatsThis(int col, const QString &text)
      {
      QStandardItem *item = itemModel->horizontalHeaderItem(col);
      item->setWhatsThis(text);
      }
