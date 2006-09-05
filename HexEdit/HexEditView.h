// HexEditView.h : interface of the CHexEditView class
//
// Copyright (c) 2004 by Andrew W. Phillips.
//
// No restrictions are placed on the noncommercial use of this code,
// as long as this text (from the above copyright notice to the
// disclaimer below) is preserved.
//
// This code may be redistributed as long as it remains unmodified
// and is not sold for profit without the author's written consent.
//
// This code, or any part of it, may not be used in any software that
// is sold for profit, without the author's written consent.
//
// DISCLAIMER: This file is provided "as is" with no expressed or
// implied warranty. The author accepts no liability for any damage
// or loss of business that this product may cause.
//

/////////////////////////////////////////////////////////////////////////////

#ifndef HEXEDITVIEW_INCLUDED
#define HEXEDITVIEW_INCLUDED  1

#include "ScrView.h"
#include "optypes.h"
// #include "Partition.h" // no longer used when schemes added
#include "range_set.h"
#include "TipWnd.h"
#include "Expr.h"

#include <algorithm>
#include <vector>
using namespace std;

// Forward declarations
class CHexEditDoc;
class CChildFrame;
class CDataFormatView;

// Different types of undo's handled by the view
enum undo_type
{
    undo_unknown = '0', // Help detect errors
    undo_change  = 'C', // Document changed (actual undo done in doc)
    undo_move    = 'M', // Caret moved
    undo_sel     = '+', // Selection (when combined with undo_move)
    undo_font    = 'F', // Font changed
    undo_setmark = 'X', // Mark set - keep track of previous mark
    undo_rowsize = 'r', // Row size (display columns) changed
    undo_group_by= 'g', // Column grouping of display changed
    undo_offset  = 'f', // Offset of display changed
    undo_highlight='I', // Change in highlighted area(s)
    undo_state   = '*', // Stores state of display etc
    undo_scheme  = 'S', // Change of colour scheme

    // These could be replaced by undo_state but are still here
    // because the first 2 do not require the screen redraw of
    // undo_state.  undo_autofit also stores the row length
    // when it is turned on, so that it can be restored.
    undo_overtype= 'o', // Overtype toggled
    undo_readonly= 'R', // Read only mode toggled
    undo_autofit = 'A', // Autofit toggled
};

#pragma pack(push, 1)
    struct view_undo
    {
        unsigned char utype:7;
        unsigned char previous_too:1; // Used to group several undo's together
        union
        {
            FILE_ADDRESS address; // Where moved from (undo_move) OR
                                // selection start (undo_move) and end (undo_sel)
            // Note top 2 bits are used to store the row for vert_display mode
            LOGFONT *plf;       // Describes previous font
            long rowsize;       // no of columns before autofit turned on
            BOOL flag;          // swap, disp, markchar etc flag
            range_set<FILE_ADDRESS> *phl; // Previous highlight set
            CString *pscheme_name; // Previous colour scheme
            DWORD disp_state;   // Saved state of the display (utype == undo_state)
        };
#ifndef NDEBUG
        int index;              // Document undo index (undo_change)
#endif

        // General constructor + default constructor (required by vector)
        view_undo(enum undo_type u = undo_unknown, BOOL p = FALSE)
            { utype = u; address = 0; previous_too = p; }
        // Copy constructor
        view_undo(const view_undo &from)
        {
            ASSERT(from.utype != undo_unknown);
            utype = from.utype; previous_too = from.previous_too;
            address = from.address;     // Naughty way to copy union
#ifndef NDEBUG
            index = from.index;
#endif
            if (from.utype == undo_font && from.plf != NULL)
            {
                plf = new LOGFONT;
                *plf = *from.plf;
            }
            else if (from.utype == undo_highlight && from.phl != NULL)
            {
                phl = new range_set<FILE_ADDRESS>;
                *phl = *from.phl;
            }
            else if (from.utype == undo_scheme && from.pscheme_name != NULL)
            {
                pscheme_name = new CString;
                *pscheme_name = *from.pscheme_name;
            }
        }
        // Assignment copy operator
        view_undo &operator=(const view_undo &from)
        {
            if (&from != this)
            {
                ASSERT(from.utype != undo_unknown);
                // First destroy existin object if nec.
                if (utype == undo_font && plf != NULL)
                    delete plf;
                else if (utype == undo_highlight && phl != NULL)
                    delete phl;
                else if (utype == undo_scheme && pscheme_name != NULL)
                    delete pscheme_name;

                // Recreate this object from assigned value
                utype = from.utype; previous_too = from.previous_too;
                if (from.utype == undo_font && from.plf != NULL)
                {
                    // Since its from free store we need to keep a separate copy
                    plf = new LOGFONT;
                    *plf = *from.plf;
                }
                else if (from.utype == undo_highlight && from.phl != NULL)
                {
                    phl = new range_set<FILE_ADDRESS>;
                    *phl = *from.phl;
                }
                else if (from.utype == undo_scheme && from.pscheme_name != NULL)
                {
                    pscheme_name = new CString;
                    *pscheme_name = *from.pscheme_name;
                }
                else
                    address = from.address;     // Naughty way to copy union
#ifndef NDEBUG
                index = from.index;
#endif
            }
            return *this;
        }
        ~view_undo()
        {
            ASSERT(utype != undo_unknown);
            // Delete any memory allocations specific to undo types
            if (utype == undo_font && plf != NULL)
                delete plf;
            else if (utype == undo_highlight && phl != NULL)
                delete phl;
            else if (utype == undo_scheme && pscheme_name != NULL)
                delete pscheme_name;
        }
        // These are required by vector<> under VC++ 5 even though not used
//        operator==(const view_undo &) const { return false; }
//        operator<(const view_undo &) const { return false; }
    };
#pragma pack(pop)

// This is used for evaluating expressions for display in tip window
class CTipExpr : public expr_eval
{
public:
    CTipExpr::CTipExpr(CHexEditView *pv) : pview_(pv), size_(0), unsigned_(false) {}
    virtual value_t find_symbol(const char *sym, value_t parent, size_t index, int *pac,
        __int64 &sym_size, __int64 &sym_address);
	int GetSize() { return size_; }
	bool GetUnsigned() { return unsigned_; }
private:
	CHexEditView *pview_;
	int size_;
	bool unsigned_;
};

// This is the main view class of the application
class CHexEditView : public CScrView
{
    friend class CHexEditApp;
    friend class CDataFormatView;
	friend class CTipExpr;

protected: // create from serialization only
        CHexEditView();
        DECLARE_DYNCREATE(CHexEditView)

// Attributes
public:
    CDataFormatView *psis_;

    enum { max_buf = 32767 };
    CHexEditDoc *GetDocument();
    CChildFrame *CHexEditView::GetFrame() const;
    CHexEditView * CHexEditView::NextView() const;

    FILE_ADDRESS GetMarkOffset() const  // offset of caret from marked position
    {
        return pos2addr(GetCaret()) - mark_;
    }
    FILE_ADDRESS GetPos() const         // Address in file of caret position
    {
        return pos2addr(GetCaret());
    }
    BOOL GetSelAddr(FILE_ADDRESS &start_addr, FILE_ADDRESS &end_addr)
    {
        ASSERT(text_height_ > 0);
        CPointAp start, end;
        BOOL retval = GetSel(start, end);
        start_addr = pos2addr(start);
        end_addr   = pos2addr(end);
        return retval;
    }


    // SetSelAddr not necessary (Use GoAddress or MoveToAddress)
//    void SetSelAddr(FILE_ADDRESS start, FILE_ADDRESS end) { SetSel(addr2pos(start), addr2pos(end)); }
    FILE_ADDRESS GetMark() { return mark_; }
    void SetMark(FILE_ADDRESS new_mark);
    BOOL ReadOnly() const { return display_.readonly; }
    BOOL OverType() const { return display_.overtype; }
    BOOL CharMode() const { return display_.edit_char; }
#if 0
    BOOL EbcdicMode() const { return display_.ebcdic; }
    BOOL OEMMode() const { return !display_.ebcdic && display_.graphic && display_.oem; }
    BOOL ANSIMode() const { return !display_.ebcdic && display_.graphic && !display_.oem; }
#else
	BOOL EbcdicMode() const { return display_.char_set == CHARSET_EBCDIC; }
    BOOL OEMMode() const { return display_.char_set == CHARSET_OEM; }
    BOOL ANSIMode() const { return display_.char_set == CHARSET_ANSI; }
#endif
    BOOL MouseDown() const { return BOOL(mouse_down_); }
    BOOL DecAddresses() const { return display_.dec_addr; }
    BOOL AutoSync() const { return display_.auto_sync; }
    void SetAutoSync(BOOL b) { display_.auto_sync = b; }
    BOOL BigEndian() const { return display_.big_endian; }
    void SetBigEndian(BOOL b) { display_.big_endian = b; }

// Operations
    void recalc_display();      // Recalculate display parameters
    void draw_bg(CDC* pDC, const CRectAp &doc_rect, bool neg_x, bool neg_y,
                 int char_height, int char_width, int char_width_w,
                 COLORREF, FILE_ADDRESS start_addr, FILE_ADDRESS end_addr,
                 int draw_height = -1);
//    void draw_bg(CDC* pDC, FILE_ADDRESS start_addr, FILE_ADDRESS end_addr, CBrush &bb);
    void do_mouse(CPoint dev_down, CSizeAp doc_dist) ;
    void do_shift_mouse(CPoint dev_down, CSizeAp doc_dist) ;
    void do_autofit(int state = -1);
    void do_chartoggle(int state = -1);
    void do_hextoggle(int state = -1);
    void do_change_state(int disp_state) { begin_change(); disp_state_ = disp_state; make_change(); end_change(); }
    void allow_mods();
    bool do_insert();
    void do_char(UINT nChar);
    void do_read(CString file_name);
    void do_motorola(CString file_name);
    void do_intel(CString file_name);
    void do_hex_text(CString file_name);
    void do_font(LOGFONT *plf);
    void do_replace(FILE_ADDRESS start, FILE_ADDRESS end, unsigned char *pp, size_t len);
    void do_insert_block(_int64 params, const char *data_str);

//    BOOL Search(const char *ss, int tt = 1, BOOL forward = TRUE);
    FILE_ADDRESS GoAddress(FILE_ADDRESS start_addr, FILE_ADDRESS end_addr = -1);
//    void SaveMove();

	// Move current position with full control of undo etc
    void MoveToAddress(FILE_ADDRESS start_addr, FILE_ADDRESS end_addr = -1, 
                       FILE_ADDRESS prev_start = -1, FILE_ADDRESS prev_end = -1, 
                       BOOL ptoo = FALSE, BOOL no_dffd = FALSE, int row = 0,
					   LPCTSTR desc = NULL);
	// Just call MoveToAddress with desc string and address, defaulting other parameters
	void MoveWithDesc(LPCTSTR desc, FILE_ADDRESS start_addr, FILE_ADDRESS end_addr = -1,
                       FILE_ADDRESS pstart = -1, FILE_ADDRESS pend = -1, 
                       BOOL ptoo = FALSE, BOOL no_dffd = FALSE, int row = 0)
	{
		MoveToAddress(start_addr, end_addr, pstart, pend, ptoo, no_dffd, row, desc);
	}

    void ToggleInsert() { OnInsert(); }
    void AllowMods() { OnAllowMods(); }
    bool CopyToClipboard();
    void do_copy_src(int src_type, int src_size, int int_type, BOOL big_endian, BOOL show_address, int indent);
    virtual BOOL MovePos(UINT nChar, UINT nRepCnt, BOOL, BOOL, BOOL);
    void StoreOptions();
    COLORREF GetDefaultTextCol() { return text_col_; }
    COLORREF GetBackgroundCol() { return bg_col_; }
    COLORREF GetDecAddrCol() { return dec_addr_col_; }
    COLORREF GetHexAddrCol() { return hex_addr_col_; }
    COLORREF GetSearchCol() { return search_col_; }
    CString GetSchemeName() { return scheme_name_; }
    void SetScheme(const char *name);
    int ClosestBookmark(FILE_ADDRESS &diff);
    void show_pos(FILE_ADDRESS address = -1, BOOL no_dffd = FALSE); // Show position (toolbar/DFFD)

public:

// Overrides
    virtual void DisplayCaret(int char_width = -1);
    CFont *SetFont(CFont *ff);

        // ClassWizard generated virtual function overrides
        //{{AFX_VIRTUAL(CHexEditView)
    public:
        virtual void OnDraw(CDC* pDC);  // overridden to draw this view
        virtual void OnInitialUpdate();
        virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = NULL);
    protected:
        virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
        virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
        virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
        virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
        virtual void OnPrint(CDC* pDC, CPrintInfo* pInfo);
        virtual void OnEndPrintPreview(CDC* pDC, CPrintInfo* pInfo, POINT point, CPreviewView* pView);
    virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    //}}AFX_VIRTUAL

// Implementation
public:
        virtual ~CHexEditView();
#ifdef _DEBUG
        virtual void AssertValid() const;
        virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    virtual void ValidateCaret(CPointAp &pos, BOOL inside=TRUE);
    virtual void ValidateScroll(CPointAp &pos, BOOL strict=FALSE);
    virtual void InvalidateRange(CPointAp start, CPointAp end);
    virtual void DoInvalidate();
    virtual void DoInvalidateRect(LPCRECT lpRect);
    virtual void DoInvalidateRgn(CRgn* pRgn);
    virtual void DoScrollWindow(int xx, int yy);
    virtual void DoUpdateWindow();
    virtual void DoHScroll(int total, int page, int pos);
    virtual void DoVScroll(int total, int page, int pos);
    void DoUpdate();

// Generated message map functions
// Make these public so they can be accessed by CHexEditApp::macro_play
// protected:
public:
        //{{AFX_MSG(CHexEditView)
        afx_msg void OnSize(UINT nType, int cx, int cy);
        afx_msg void OnAddrToggle();
        afx_msg void OnUpdateAddrToggle(CCmdUI* pCmdUI);
        afx_msg void OnGraphicToggle();
        afx_msg void OnUpdateGraphicToggle(CCmdUI* pCmdUI);
        afx_msg void OnCharToggle();
        afx_msg void OnUpdateCharToggle(CCmdUI* pCmdUI);
        afx_msg void OnFont();
        afx_msg void OnAutoFit();
        afx_msg void OnUpdateAutofit(CCmdUI* pCmdUI);
        afx_msg void OnAscEbc();
        afx_msg void OnUpdateAscEbc(CCmdUI* pCmdUI);
        afx_msg void OnControl();
        afx_msg void OnUpdateControl(CCmdUI* pCmdUI);
        afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
        afx_msg void OnSetFocus(CWnd* pOldWnd);
        afx_msg void OnKillFocus(CWnd* pNewWnd);
        afx_msg void OnDestroy();
        afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
        afx_msg void OnMark();
        afx_msg void OnGotoMark();
        afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
        afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
        afx_msg void OnEditUndo();
        afx_msg void OnSearchHex();
        afx_msg void OnSearchAscii();
        afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
        afx_msg void OnAllowMods();
        afx_msg void OnUpdateAllowMods(CCmdUI* pCmdUI);
        afx_msg void OnControlToggle();
        afx_msg void OnInsert();
        afx_msg void OnUpdateInsert(CCmdUI* pCmdUI);
        afx_msg void OnSearchIcase();
        afx_msg void OnEditCompare();
        afx_msg void OnWindowNext();
        afx_msg void OnUpdateEditCompare(CCmdUI* pCmdUI);
        afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
        afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
        afx_msg void OnIncByte();
        afx_msg void OnInc16bit();
        afx_msg void OnInc32bit();
        afx_msg void OnInc64bit();
        afx_msg void OnDecByte();
        afx_msg void OnDec16bit();
        afx_msg void OnDec32bit();
        afx_msg void OnDec64bit();
        afx_msg void OnFlip16bit();
        afx_msg void OnFlip32bit();
        afx_msg void OnFlip64bit();
        afx_msg void OnUpdateByte(CCmdUI* pCmdUI);
        afx_msg void OnUpdate16bit(CCmdUI* pCmdUI);
        afx_msg void OnUpdate32bit(CCmdUI* pCmdUI);
        afx_msg void OnUpdate64bit(CCmdUI* pCmdUI);
        afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
        afx_msg void OnSelectAll();
        afx_msg void OnEditCopy();
        afx_msg void OnEditCut();
        afx_msg void OnEditPaste();
        afx_msg void OnUpdateTextPaste(CCmdUI* pCmdUI);
        afx_msg void OnUpdateClipboard(CCmdUI* pCmdUI);
        afx_msg void OnUpdateUnicodePaste(CCmdUI* pCmdUI);
        afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
        afx_msg void OnFontDec();
        afx_msg void OnFontInc();
        afx_msg void OnUpdateFontDec(CCmdUI* pCmdUI);
        afx_msg void OnUpdateFontInc(CCmdUI* pCmdUI);
        afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
        afx_msg void OnPasteAscii();
        afx_msg void OnPasteEbcdic();
        afx_msg void OnPasteUnicode();
        afx_msg void OnCopyCchar();
        afx_msg void OnCopyHex();
        afx_msg void OnEditWriteFile();
        afx_msg void OnUpdateReadFile(CCmdUI* pCmdUI);
        afx_msg void OnReadFile();
        afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
        afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
        afx_msg void OnExtendToMark();
        afx_msg void OnSwapMark();
        afx_msg void OnRedraw();
        afx_msg void OnScrollDown();
        afx_msg void OnScrollUp();
        afx_msg void OnSwap();
        afx_msg void OnStartLine();
        afx_msg void OnDel();
        afx_msg void OnUpdateSwap(CCmdUI* pCmdUI);
        afx_msg void OnOemToggle();
        afx_msg void OnUpdateOemToggle(CCmdUI* pCmdUI);
        afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
        afx_msg void OnInvert();
        afx_msg void OnNegByte();
        afx_msg void OnNeg16bit();
        afx_msg void OnNeg32bit();
        afx_msg void OnNeg64bit();
        afx_msg void OnMouseMove(UINT nFlags, CPoint point);
        afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
        afx_msg void OnHighlight();
        afx_msg void OnUpdateHighlight(CCmdUI* pCmdUI);
        afx_msg void OnHighlightClear();
        afx_msg void OnHighlightPrev();
        afx_msg void OnHighlightNext();
        afx_msg void OnUpdateHighlightPrev(CCmdUI* pCmdUI);
        afx_msg void OnUpdateHighlightNext(CCmdUI* pCmdUI);
        afx_msg void OnEditGoto();
    afx_msg void OnAscii2Ebcdic();
    afx_msg void OnUpdateConvert(CCmdUI* pCmdUI);
    afx_msg void OnEbcdic2Ascii();
    afx_msg void OnAnsi2Ibm();
    afx_msg void OnIbm2Ansi();
    afx_msg void OnEncrypt();
    afx_msg void OnDecrypt();
    afx_msg void OnUpdateEncrypt(CCmdUI* pCmdUI);
    afx_msg void OnEditAppendFile();
    afx_msg void OnEditAppendSameFile();
    afx_msg void OnUpdateEditAppendSameFile(CCmdUI* pCmdUI);
    afx_msg void OnUndoChanges();
    afx_msg void OnUpdateUndoChanges(CCmdUI* pCmdUI);
    afx_msg void OnCalcSel();
    afx_msg void OnUpdateCalcSel(CCmdUI* pCmdUI);
    afx_msg void OnDisplayReset();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnImportMotorolaS();
    afx_msg void OnUpdateImportMotorolaS(CCmdUI* pCmdUI);
    afx_msg void OnImportIntel();
    afx_msg void OnUpdateImportIntel(CCmdUI* pCmdUI);
    afx_msg void OnExportIntel();
    afx_msg void OnUpdateExportIntel(CCmdUI* pCmdUI);
    afx_msg void OnImportHexText();
    afx_msg void OnUpdateImportHexText(CCmdUI* pCmdUI);
    afx_msg void OnExportHexText();
    afx_msg void OnUpdateExportHexText(CCmdUI* pCmdUI);
    afx_msg void OnCrc32();
    afx_msg void OnCrcCcitt();
    afx_msg void OnBookmarksHide();
    afx_msg void OnUpdateBookmarksHide(CCmdUI* pCmdUI);
    afx_msg void OnHighlightHide();
    afx_msg void OnUpdateHighlightHide(CCmdUI* pCmdUI);
    afx_msg void OnBookmarksNext();
    afx_msg void OnUpdateBookmarksNext(CCmdUI* pCmdUI);
    afx_msg void OnBookmarksPrev();
    afx_msg void OnUpdateBookmarksPrev(CCmdUI* pCmdUI);
    afx_msg void OnBookmarksClear();
    afx_msg void OnUpdateBookmarksClear(CCmdUI* pCmdUI);
    afx_msg void OnEditFind();
    afx_msg void OnBookmarkToggle();
    afx_msg void OnColumnDec();
    afx_msg void OnUpdateColumnDec(CCmdUI* pCmdUI);
    afx_msg void OnColumnInc();
    afx_msg void OnUpdateColumnInc(CCmdUI* pCmdUI);
    afx_msg void OnDffdAutoSync();
    afx_msg void OnUpdateDffdAutoSync(CCmdUI* pCmdUI);
    afx_msg void OnDffdSync();
    afx_msg void OnUpdateDffdSync(CCmdUI* pCmdUI);
    afx_msg void OnSeedCalc();
    afx_msg void OnSeedRandom();
    afx_msg void OnTrackChanges();
    afx_msg void OnUpdateTrackChanges(CCmdUI* pCmdUI);
    //}}AFX_MSG
    afx_msg void OnUpdateSelNZ(CCmdUI* pCmdUI);  // no-zero selection

    afx_msg void OnEditReplace();
    afx_msg void OnChecksum8();
    afx_msg void OnChecksum16();
    afx_msg void OnChecksum32();
    afx_msg void OnChecksum64();
        afx_msg void OnUpdateByteNZ(CCmdUI* pCmdUI);
        afx_msg void OnUpdate16bitNZ(CCmdUI* pCmdUI);
        afx_msg void OnUpdate32bitNZ(CCmdUI* pCmdUI);
        afx_msg void OnUpdate64bitNZ(CCmdUI* pCmdUI);

        afx_msg void OnXorByte();
        afx_msg void OnXor16bit();
        afx_msg void OnXor32bit();
        afx_msg void OnXor64bit();
        afx_msg void OnAssignByte();
        afx_msg void OnAssign16bit();
        afx_msg void OnAssign32bit();
        afx_msg void OnAssign64bit();
        afx_msg void OnRandByte();
        afx_msg void OnRandFast();
//        afx_msg void OnRand16bit();
//        afx_msg void OnRand32bit();
//        afx_msg void OnRand64bit();
        afx_msg void OnAddByte();
        afx_msg void OnAdd16bit();
        afx_msg void OnAdd32bit();
        afx_msg void OnAdd64bit();
        afx_msg void OnSubtractByte();
        afx_msg void OnSubtract16bit();
        afx_msg void OnSubtract32bit();
        afx_msg void OnSubtract64bit();
        afx_msg void OnAndByte();
        afx_msg void OnAnd16bit();
        afx_msg void OnAnd32bit();
        afx_msg void OnAnd64bit();
        afx_msg void OnOrByte();
        afx_msg void OnOr16bit();
        afx_msg void OnOr32bit();
        afx_msg void OnOr64bit();

        afx_msg void OnMulByte();
        afx_msg void OnMul16bit();
        afx_msg void OnMul32bit();
        afx_msg void OnMul64bit();
        afx_msg void OnDivByte();
        afx_msg void OnDiv16bit();
        afx_msg void OnDiv32bit();
        afx_msg void OnDiv64bit();
        afx_msg void OnModByte();
        afx_msg void OnMod16bit();
        afx_msg void OnMod32bit();
        afx_msg void OnMod64bit();
        afx_msg void OnRevByte();
        afx_msg void OnRev16bit();
        afx_msg void OnRev32bit();
        afx_msg void OnRev64bit();
        afx_msg void OnSubtractXByte();
        afx_msg void OnSubtractX16bit();
        afx_msg void OnSubtractX32bit();
        afx_msg void OnSubtractX64bit();
        afx_msg void OnDivXByte();
        afx_msg void OnDivX16bit();
        afx_msg void OnDivX32bit();
        afx_msg void OnDivX64bit();
        afx_msg void OnModXByte();
        afx_msg void OnModX16bit();
        afx_msg void OnModX32bit();
        afx_msg void OnModX64bit();
        afx_msg void OnGtrByte();
        afx_msg void OnGtr16bit();
        afx_msg void OnGtr32bit();
        afx_msg void OnGtr64bit();
        afx_msg void OnLessByte();
        afx_msg void OnLess16bit();
        afx_msg void OnLess32bit();
        afx_msg void OnLess64bit();
        afx_msg void OnGtrUByte();
        afx_msg void OnGtrU16bit();
        afx_msg void OnGtrU32bit();
        afx_msg void OnGtrU64bit();
        afx_msg void OnLessUByte();
        afx_msg void OnLessU16bit();
        afx_msg void OnLessU32bit();
        afx_msg void OnLessU64bit();

        afx_msg void OnRolByte();
        afx_msg void OnRol16bit();
        afx_msg void OnRol32bit();
        afx_msg void OnRol64bit();
        afx_msg void OnRorByte();
        afx_msg void OnRor16bit();
        afx_msg void OnRor32bit();
        afx_msg void OnRor64bit();
        afx_msg void OnLslByte();
        afx_msg void OnLsl16bit();
        afx_msg void OnLsl32bit();
        afx_msg void OnLsl64bit();
        afx_msg void OnLsrByte();
        afx_msg void OnLsr16bit();
        afx_msg void OnLsr32bit();
        afx_msg void OnLsr64bit();
        afx_msg void OnAsrByte();
        afx_msg void OnAsr16bit();
        afx_msg void OnAsr32bit();
        afx_msg void OnAsr64bit();

        afx_msg void OnUpdateByteBinary(CCmdUI* pCmdUI);
        afx_msg void OnUpdate16bitBinary(CCmdUI* pCmdUI);
        afx_msg void OnUpdate32bitBinary(CCmdUI* pCmdUI);
        afx_msg void OnUpdate64bitBinary(CCmdUI* pCmdUI);
        afx_msg void OnExportSRecord(UINT nID);
        afx_msg void OnUpdateExportSRecord(CCmdUI* pCmdUI);

        afx_msg void OnDisplayHex();
        afx_msg void OnUpdateDisplayHex(CCmdUI *pCmdUI);
        afx_msg void OnDisplayChar();
        afx_msg void OnUpdateDisplayChar(CCmdUI *pCmdUI);
        afx_msg void OnDisplayBoth();
        afx_msg void OnUpdateDisplayBoth(CCmdUI *pCmdUI);
        afx_msg void OnDisplayStacked();
        afx_msg void OnUpdateDisplayStacked(CCmdUI *pCmdUI);
        afx_msg void OnCharsetAscii();
        afx_msg void OnUpdateCharsetAscii(CCmdUI *pCmdUI);
        afx_msg void OnCharsetAnsi();
        afx_msg void OnUpdateCharsetAnsi(CCmdUI *pCmdUI);
        afx_msg void OnCharsetOem();
        afx_msg void OnUpdateCharsetOem(CCmdUI *pCmdUI);
        afx_msg void OnCharsetEbcdic();
        afx_msg void OnUpdateCharsetEbcdic(CCmdUI *pCmdUI);
        afx_msg void OnControlNone();
        afx_msg void OnUpdateControlNone(CCmdUI *pCmdUI);
        afx_msg void OnControlAlpha();
        afx_msg void OnUpdateControlAlpha(CCmdUI *pCmdUI);
        afx_msg void OnControlC();
        afx_msg void OnUpdateControlC(CCmdUI *pCmdUI);

        afx_msg void OnToggleEndian();
        afx_msg void OnBigEndian();
        afx_msg void OnLittleEndian();
        afx_msg void OnUpdateToggleEndian(CCmdUI* pCmdUI);
        afx_msg void OnUpdateBigEndian(CCmdUI* pCmdUI);
        afx_msg void OnUpdateLittleEndian(CCmdUI* pCmdUI);

        afx_msg void OnFilePrint();
        afx_msg void OnFilePrintPreview();
        afx_msg void OnJumpHexAddr();
//        afx_msg void OnSearch();
        afx_msg void OnJumpHex();
        afx_msg void OnJumpDec();
        afx_msg void OnSelectLine();

        afx_msg void OnFontName();
        afx_msg void OnUpdateFontName(CCmdUI* pCmdUI);
        afx_msg void OnFontSize();
        afx_msg void OnUpdateFontSize(CCmdUI* pCmdUI);

    afx_msg void OnCompress();
    afx_msg void OnDecompress();

        //afx_msg void OnTimer(UINT nIDEvent);
        afx_msg void OnInsertBlock();
        afx_msg void OnUpdateInsertBlock(CCmdUI* pCmdUI);
        afx_msg void OnViewtest();
        afx_msg void OnSysColorChange();
        afx_msg LRESULT OnMouseHover(WPARAM wp, LPARAM lp);
        afx_msg LRESULT OnMouseLeave(WPARAM wp, LPARAM lp);

        DECLARE_MESSAGE_MAP()

public:
    // Had to make this public to be accessed from template functions (OnOperateBinary and
    // OnOperateUnary), which I couldn't make members or even friends - useless compiler!!
    BOOL check_ro(const char *); // Check if file is read only when changes attempted
    CPointAp addr2pos(FILE_ADDRESS address, int row = 0) const; // Convert byte address in doc to display position

	void check_error();          // Check for read errors and mention them to the user

private:
    enum { max_font_size = 100 };
#ifdef _DEBUG
    // Use smaller buffers to make it more likely to catch certain types of bugs
    enum { search_buf_len = 128, compare_buf_len = 64, clipboard_buf_len = 1024 };
#else
    enum { search_buf_len = 32768, compare_buf_len = 4096, clipboard_buf_len = 1024 };
#endif
    int hex_pos(int column, int width=0) const // get X coord of hex display column
    {
        ASSERT(column <= rowsize_);
        //ASSERT(!display_.vert_display || column == 0);
        if (width == 0) width = text_width_;
        return (addr_width_ + column*3 + column/group_by_)*width;
    }
    int char_pos(int column, int widthd=0, int widthw=0) const // get X coord of ASCII/EBCDIC display column
    {
        ASSERT(column <= rowsize_);
        if (widthd == 0) widthd = text_width_;
        if (widthw == 0) widthw = text_width_w_;
        if (display_.vert_display)
            return addr_width_*widthd + (column + column/group_by_)*widthw;
        else if (display_.hex_area)
            return (addr_width_ + rowsize_*3)*widthd + ((rowsize_-1)/group_by_)*widthd + column*widthw;
        else
            return addr_width_*widthd + column*widthw;
    }
    int pos_hex(int, BOOL inside = FALSE) const;  // Closest hex display col given X
    int pos_char(int, BOOL inside = FALSE) const; // Closest char area col given X
    FILE_ADDRESS pos2addr(CPointAp pos, BOOL inside = TRUE) const; // Convert a display position to closest address
    int pos2row(CPointAp pos);                    // Find vert_display row (0, 1, or 2) of display position
    void show_prop(FILE_ADDRESS address = -1); // Show current properties (modeless dlg)
    void show_calc();           // Show view info in state of calculator buttons

	// Functions for selection tip (sel_tip_)
//    void show_selection_tip();
    void reset_tip();
    void update_sel_tip(int delay = 0);

    void add_highlight(FILE_ADDRESS start, FILE_ADDRESS end, BOOL ptoo=FALSE);
    void invalidate_addr_range(FILE_ADDRESS, FILE_ADDRESS); // Invalidate display for address range
    BOOL is_last_change();      // Is top op on undo stack a file change
    BOOL do_undo();             // Undo one from top of undo stack
//    void load_bitmaps();        // Redraw buttons for this view
//    BOOL do_search(BOOL forward = TRUE);
//    BOOL search_forw(const unsigned char *ss, const unsigned char *mask, size_t, BOOL, int, BOOL, int);
//    BOOL search_back(const unsigned char *ss, const unsigned char *mask, size_t, BOOL, int, BOOL, int);
    void view_context(CPoint point) ; // Display context menu
    CChildFrame *comp_window(); // Compare windows
    void move_dlgs();           // Move modeless dialogs if they cover the caret
    void change_rowsize(int rowsize);
    void change_group_by(int group_by);
    void change_offset(int offset);
    BOOL set_colours();         // Set colours from app schemes using current scheme_name_
    void redo_font();

    CSizeAp win_size_;          // Current window dimensions (doc coords)
    CFont *pfont_;              // Current display font
    CBrush *pbrush_;            // Current brush (used with PatBlt)
    LOGFONT lf_, oem_lf_;       // Normal font and font for IBM/OEM graphics chars
    CString fontsize_;          // Use to update the font size tool (format toolbar)

// Move this to OnDraw since reentrant OnDraw calls cause a problem
//    unsigned char buf_[max_buf];// Holds bytes for current line being displayed
    int rowsize_;               // Number of bytes per row
    int offset_;                // offset of start of row (0 to rowsize_ - 1)
    int real_offset_;           // Stores actual last set offset which may be different (larger)
                                // than offset_ when autofit is on & # of cols has been reduced
    int group_by_;              // How many columns to group together

    int text_height_;           // How high are characters?
    int text_width_;            // How wide is widest hex char (D)?
    int text_width_w_;          // How wide is widest char (W)?
    int line_height_;           // Same as text_height_ unless vert_display mode (= 3*text_height_)
    BCHAR first_char_, last_char_; // Range of chars in current font
    int addr_width_;            // How much room in display does address take?
    const char *addr_format_;   // What format string to use for address

    FILE_ADDRESS mark_;         // Address of marked position

    // The display state is now stored in one 32 bit value for ease of saving the state
    // in undo vector and in registry
    union
    {
        DWORD disp_state_;
        struct display_bits display_;
    };

    // The following are saved before making changes to options that change the
    // display (in begin_change) and then used in end_change.
    DWORD previous_state_, saved_state_;
    BOOL previous_caret_displayed_;
    FILE_ADDRESS previous_start_addr_, previous_end_addr_; // selection
    BOOL previous_end_base_;
    int previous_row_;                          // row (0-2) if vert_display
    void begin_change();                        // Store current state etc
    BOOL make_change(BOOL ptoo = FALSE);        // Save state to undo
    void end_change();                          // Fix display etc

    // Display colours
    COLORREF bg_col_, mark_col_, hi_col_, bm_col_, search_col_, text_col_;
    COLORREF trk_col_, trk_bg_col_;             // Change tracking colours
    COLORREF addr_bg_col_, dec_addr_col_, hex_addr_col_;
	COLORREF sector_col_, sector_bg_col_;       // Displays sector bondaries and bad sector background

    CString scheme_name_;
    COLORREF kala[256];         // Actual colours for each byte value

    // Current bg search position displayed
//    std::vector<FILE_ADDRESS> search_found_;
    // We store the found occurrences as areas rather than as addresses since if there
    // are a lot of overlapping occurrences it can be very slow to redraw the background
    // of the same areas several times.  By storing the areas (which may vary in length)
    // we can quickly draw overlapping search occurrences.
    std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > search_pair_;  // Areas that need to be drawn to indicate found occurrences
    int search_length_;

    // Current highlighted ranges
    range_set<FILE_ADDRESS> hl_set_;

    // Current edit position and state
    short num_entered_;         // How many consecutive characters entered?
    short num_del_;             // How many consec. chars deleted?
    short num_bs_;              // How many consec. back spaces?

	CTipWnd sel_tip_;           // Selection tip (shown when selecting or when SHIFT held down)

    // The following are saved when a new selection is started and are used when
    // it is finished to see what undo info. we need to save
    FILE_ADDRESS prev_start_, prev_end_; // Previous selection
    int prev_row_;              // Row if in vert_display mode
    bool mouse_down_;           // Left button down seen but not yet up

    // Note some declarations are bool because this is the new C++ standard,
    // others are BOOL to avoid warnings.  One day I'll change them all to
    // bool and add typecasts where necessary.

    // These are used to display tips (eg for bookmarks)
	CTipWnd tip_;               // Tip when mouse hovers (just bookmark name for now)
	FILE_ADDRESS tip_addr_;     // The address we display info for in the tip window
	bool tip_show_bookmark_;    // Show bookmark name if over bookmark
	bool tip_show_error_;       // Show error message if current sector had a read error
	CTipExpr expr_;             // Used to evaluate expressions to display in the tip window

    //UINT timer_id_;             // Used for timing of display of tip window
    //CPoint last_mouse_;         // Last mouse move position
	bool update_tip(FILE_ADDRESS addr); // Update tip text using an address of the file
    FILE_ADDRESS address_at(CPoint pt); // Given a position return file address (dep on area of point)
    FILE_ADDRESS last_tip_addr_;
    void track_mouse(unsigned long); // Turns on receipt of mouse hover/leave messages
    int bookmark_at(FILE_ADDRESS); // Return bookmark at position or -1 if none

    // Info required for macro (km_mouse and km_shift_mouse)
    CPoint dev_down_;           // Mouse down (device coordinates)
    CPointAp doc_down_;         // Mouse down (doc coords)
    BOOL shift_down_;           // Shift key was down when left button down seen
    int shift_moves_;           // Counts shift-select cursor keys since shift down

    // Display refresh stuff for when running macros
    bool needs_refresh_;        // Does window need redrawing when refresh turned on again
    bool needs_hscroll_;        // Does the horizontal scroll bar need updating
    int h_total_, h_page_, h_pos_; // Last parameters for DoHScroll
    bool needs_vscroll_;        // Does the vertical scroll bar need updating
    int v_total_, v_page_, v_pos_; // Last parameters for DoVScroll

    // Undo array (things done in this view that don't change doc)
    std::vector <view_undo> undo_;
    FILE_ADDRESS saved_start_, saved_end_;

	// Info for navigation backward/forward commands
    CString get_info();
	void nav_save(FILE_ADDRESS astart, FILE_ADDRESS aend, LPCTSTR desc);
	FILE_ADDRESS nav_start_, nav_end_; // Last nav point in this view
	FILE_ADDRESS nav_scroll_;   // Window scroll position at nav pt

    // Printing info
    CString create_header(const char *fmt, long pagenum);
    CFont *print_font_;         // Font used when printing (= screen font scaled for printer)
    CTime print_time_;          // Print time in headers and footers
    int curpage_;               // Current page being printed
    bool print_sel_;            // Are we printing the current selection?
    int lines_per_page_;        // Number of text lines on a printed page
    CSize page_size_;           // Size of printer page in logical units
    CSize margin_size_;         // Size of printer margins (top/left)
    int print_map_mode_;        // Mapping mode for printer (not MM_TEXT so that text is scaled)
    long print_lfHeight_;       // Logical font height in logical (print_map_mode_) units
    // Note: print_text_height_ is analogous to text_height_ but for printing.
    // In OnDraw these values may be different since screen text is drawn using MM_TEXT
    // but printer text uses MM_HIMETRIC so that the text is not tiny on a laser.
    int print_text_height_ ;    // Height of chars when printing
    int print_text_width_;      // Width of hex chars in printing DC
    int print_text_width_w_;    // Width of chars in printing DC
    CSize print_offset_;        // How far to offset the printed area (dep. on diff. print options)

    BOOL dup_lines_;            // Merge duplicate lines?
    FILE_ADDRESS print_next_line_; // First line of the next page if dup_lines_ is on

    static const char *bin_format_name; // Name of our custom clipboard format

	bool errors_mentioned_;    // If there are errors have we mentioned it to the user?
};

#ifndef _DEBUG  // debug version in HexEditView.cpp
inline CHexEditDoc* CHexEditView::GetDocument()
   { return (CHexEditDoc*)m_pDocument; }
#endif


#endif // HEXEDITVIEW_INCLUDED

/////////////////////////////////////////////////////////////////////////////