/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 *
 * Copyright: 2012-2013 Boomaga team https://github.com/Boomaga
 * Authors:
 *   Alexander Sokoloff <sokoloff.a@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */


#include "layout.h"
#include <QString>
#include "project.h"
#include "sheet.h"
#include "printer.h"

#include "math.h"

#include <QDebug>
/************************************************

 ************************************************/
Layout::Layout()
{
}


/************************************************

 ************************************************/
Layout::~Layout()
{
}


/************************************************

 ************************************************/
void Layout::fillPreviewSheets(QList<Sheet *> *sheets) const
{
    fillSheets(sheets);
}


/************************************************

 ************************************************/
LayoutNUp::LayoutNUp(int pageCountVert, int pageCountHoriz, Qt::Orientation orientation):
    Layout(),
    mPageCountVert(pageCountVert),
    mPageCountHoriz(pageCountHoriz),
    mOrientation(orientation)
{
}


/************************************************

 ************************************************/
QString LayoutNUp::id() const
{
    return QString("%1up%2")
            .arg(mPageCountVert * mPageCountHoriz)
            .arg((mOrientation == Qt::Horizontal ? "" : "V"));
}


/************************************************

 ************************************************/
void LayoutNUp::fillSheets(QList<Sheet *> *sheets) const
{
    int pps = mPageCountVert * mPageCountHoriz;

    int i=0;
    while (i < project->pageCount())
    {
        Sheet *sheet = new Sheet(pps, sheets->count());

        for (int j=0; j<pps; ++j)
        {
            if (i<project->pageCount())
            {
                ProjectPage *page = project->page(i);
                sheet->setPage(j, page);
            }
            ++i;
        }

        sheet->setRotation(calcSheetRotation(*sheet));
        sheets->append(sheet);
    }
}


/************************************************
 * In this place sheet is always has portrait orientation.
 * It will be rotated later based on sheet.rotation() value.
 ************************************************/
TransformSpec LayoutNUp::transformSpec(const Sheet *sheet, int pageNumOnSheet) const
{
    Printer *printer = project->printer();
    const QRectF printerRect = printer->pageRect();
    const qreal margin = printer->internalMarhin();

    const qreal colWidth  = (printerRect.width()  - margin * (mPageCountHoriz - 1)) / mPageCountHoriz;
    const qreal rowHeight = (printerRect.height() - margin * (mPageCountVert  - 1)) / mPageCountVert;

    uint col, row;
    {
        PagePosition colRow = calcPagePosition(sheet, pageNumOnSheet);
        col = colRow.col;
        row = colRow.row;
    }

    // ................................
    const ProjectPage *page = sheet->page(pageNumOnSheet);

    QSizeF pdfPageSize = page ?
                sheet->page(pageNumOnSheet)->rect().size() :
                pdfPageSize = printer->paperRect().size();


    Rotation rotatePage = page ?
                sheet->rotation() - page->rotation() :
                sheet->rotation();

    if (isLandscape(rotatePage))
         pdfPageSize.transpose();
    // ................................

    qreal scale = qMin(colWidth  / pdfPageSize.width(),
                       rowHeight / pdfPageSize.height());

    QRectF placeRect;
    placeRect.setWidth( pdfPageSize.width() * scale);
    placeRect.setHeight(pdfPageSize.height() * scale);
    QPointF center;
    //            |  TopLeft page    |  margins       | full pages        | center of current page
    center.rx() = printerRect.left() + (margin * col) + (colWidth  * col) + (colWidth  * 0.5);
    center.ry() = printerRect.top()  + (margin * row) + (rowHeight * row) + (rowHeight * 0.5);
    placeRect.moveCenter(center);

    TransformSpec spec;
    spec.rect = placeRect;
    spec.rotation = rotatePage;
    spec.scale = scale;

    return spec;
}


/************************************************

 ************************************************/
Rotation LayoutNUp::rotation() const
{
    if (mPageCountVert != mPageCountHoriz)
        return Rotate90;
    else
        return NoRotate;
}


/************************************************
 *
 ************************************************/
Rotation LayoutNUp::calcSheetRotation(const Sheet &sheet) const
{
    Rotation layoutRotation = project->layout()->rotation();

    for (int i=0; i< sheet.count(); ++i)
    {
        const ProjectPage *page = sheet.page(i);
        if (page)
        {
            return layoutRotation - page->rotation();
        }
    }

    return layoutRotation;
}


/************************************************
 * In this place sheet is always has portrait orientation.
 * It will be rotated later based on sheet.rotation() value
 *
 *  h - mPageCountHoriz
 *  v - mPageCountVert
 *
 *  +------+ Rotate: 0           +------+ Rotate: 0
 *  | 0  1 | Horiz               | 0  4 | Vert
 *  | 2  3 |                     | 1  5 |
 *  | 4  5 | r = i / h           | 2  6 | r = i % v
 *  | 6  7 | c = i % h           | 3  7 | c = i / v
 *  +------+                     +------+
 *
 *  +------+ Rotate: 90          +------+ Rotate: 90
 *  | 3  7 | Horiz               | 6  7 | Vert
 *  | 2  6 |                     | 4  5 |
 *  | 1  5 | r = (v-1) - i % v   | 2  3 | r = (v-1) - i / h
 *  | 0  4 | c = i / v           | 0  1 | c = i % h
 *  +------+                     +------+
 *
 *  +------+ Rotate: 180         +------+ Rotate: 180
 *  | 7  6 | Horiz               | 7  3 | Vert
 *  | 5  4 |                     | 6  2 |
 *  | 3  2 | r = (v-1) - R0H.r   | 5  1 | r = (v-1) - R0V.r
 *  | 1  0 | c = (h-1) - R0H.c   | 4  0 | c = (h-1) - R0V.c
 *  +------+                     +------+
 *
 *  +------+ Rotate: 270         +------+ Rotate: 270
 *  | 4  0 | Horiz               | 1  0 | Vert
 *  | 5  1 |                     | 3  2 |
 *  | 6  2 | r = (v-1) - R90H.r  | 5  4 | r = (v-1) - R90V.r
 *  | 7  3 | c = (h-1) - R90H.c  | 7  6 | c = (h-1) - R90V.c
 *  +------+                     +------+
 *
 ************************************************/
Layout::PagePosition LayoutNUp::calcPagePosition(const Sheet *sheet, int pageNumOnSheet) const
{
    Rotation rotate = sheet->rotation();
    PagePosition res;

    if (isLandscape(rotate))
    {
        // 90 or 270
        switch (mOrientation)
        {
        case Qt::Horizontal:
            res.row = (mPageCountVert - 1) - pageNumOnSheet % mPageCountVert;
            res.col = pageNumOnSheet / mPageCountVert;
            break;

        case Qt::Vertical:
            res.row = (mPageCountVert - 1) - pageNumOnSheet / mPageCountHoriz;
            res.col = pageNumOnSheet % mPageCountHoriz;
            break;
        }
    }
    else
    {
        // 0 or 180
        switch (mOrientation)
        {
        case Qt::Horizontal:
            res.row = pageNumOnSheet / mPageCountHoriz;
            res.col = pageNumOnSheet % mPageCountHoriz;
            break;

        case Qt::Vertical:
            res.row = pageNumOnSheet % mPageCountVert;
            res.col = pageNumOnSheet / mPageCountVert;
            break;
        }

    }


    if (rotate == Rotate180 ||
        rotate == Rotate270 )
    {
        res.row = (mPageCountVert  - 1) - res.row;
        res.col = (mPageCountHoriz - 1) - res.col;
    }

    return res;
}


/************************************************

 ************************************************/
LayoutBooklet::LayoutBooklet():
    LayoutNUp(2, 1)
{
}


/************************************************

 ************************************************/
void LayoutBooklet::fillSheets(QList<Sheet *> *sheets) const
{
    fillSheetsForBook(0, project->pageCount(), sheets);
}


/************************************************

  +-----------+  +-----------+
  |     :     |  |     :     |
  |  N  :  0  |  |  1  : N-1 |
  |     :     |  |     :     |
  +-----------+  +-----------+
     sheet 0        sheet 1

  +-----------+  +-----------+
  |     :     |  |     :     |
  | N-3 :  3  |  |  4  : N-4 |
  |     :     |  |     :     |
  +-----------+  +-----------+
     sheet 2        sheet 3
               ...
 ************************************************/
void LayoutBooklet::fillSheetsForBook(int bookStart, int bookLength, QList<Sheet *> *sheets) const
{
    int cnt = ceil(bookLength / 4.0 ) * 4;

    for (int i = 0; i < cnt / 2; i+=2)
    {
        // Sheet 0 **************************
        Sheet *sheet = new Sheet(2, sheets->count());
        sheet->setHints(Sheet::HintDrawFold);
        sheets->append(sheet);

        int n = (cnt - 1) - i;
        if (n < bookLength)
        {
            ProjectPage *page = project->page(n + bookStart);
            sheet->setPage(0, page);
        }


        n = i;
        if (n < bookLength)
        {
            ProjectPage *page = project->page(n + bookStart);
            sheet->setPage(1, page);
        }

        sheet->setRotation(calcSheetRotation(*sheet));

        // Sheet 1 **************************
        sheet = new Sheet(2, sheets->count());
        sheet->setHints(Sheet::HintDrawFold);
        sheets->append(sheet);

        n = i + 1;
        if (n < bookLength)
        {
            ProjectPage *page = project->page(n + bookStart);
            sheet->setPage(0, page);
        }

        n = (cnt - 1) - (i + 1);
        if (n < bookLength)
        {
            ProjectPage *page = project->page(n + bookStart);
            sheet->setPage(1, page);
        }

        sheet->setRotation(calcSheetRotation(*sheet));
    }
}


/************************************************

 ************************************************/
void LayoutBooklet::fillPreviewSheets(QList<Sheet *> *sheets) const
{
    fillPreviewSheetsForBook(0, project->pageCount(), sheets);
    if (sheets->count() > 1)
    {
        sheets->first()->setHint(Sheet::HintOnlyRight, true);
        sheets->first()->setHint(Sheet::HintDrawFold, false);
        sheets->last()->setHint(Sheet::HintOnlyLeft, true);
        sheets->last()->setHint(Sheet::HintDrawFold, false);
    }
}


/************************************************
  : - - +-----+  +-----------+
  :     |     |  |     :     |
  : -1  |  0  |  |  1  :  2  |
  :     |     |  |     :     |
  : - - +-----+  +-----------+
     sheet 0        sheet 1

  +-----------+  +-----+ - - :
  |     :     |  |     |     :
  |  3  :  4  |  |  5  |     :
  |     :     |  |     |     :
  +-----------+  +-----+ - - :
     sheet 2        sheet 3

 ************************************************/
void LayoutBooklet::fillPreviewSheetsForBook(int bookStart, int bookLength, QList<Sheet *> *sheets) const
{
    int cnt = ceil(bookLength / 4.0 ) * 4;

    for (int i = -1; i < cnt; i+=2)
    {
        Sheet *sheet = new Sheet(2, sheets->count());
        sheet->setHints(Sheet::HintDrawFold);

        sheets->append(sheet);
        if (i>-1)
        {
            if (i<project->pageCount())
            {
                ProjectPage *page = project->page(i + bookStart);
                sheet->setPage(0, page);
                sheet->setRotation(calcSheetRotation(*sheet));
            }
        }

        if (i+1<cnt)
        {
            if (i+1<project->pageCount())
            {
                ProjectPage *page = project->page(i + 1 + bookStart);
                sheet->setPage(1, page);
                sheet->setRotation(calcSheetRotation(*sheet));
            }
        }
    }
}
