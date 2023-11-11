#ifndef TEXTEDIT_H_
#define TEXTEDIT_H_

struct EditLine
{
    enum { Chunk_Size = 256 };

    char *text;
    int len, maxlen;

    EditLine() : text(nullptr), len(0), maxlen(0) {}
    EditLine(const char *init) : text(nullptr), len(0), maxlen(0)
    {
        set(init);
    }

    bool empty();
    void clear();
    bool grow(int total, const char *fmt = "", ...);
    void set(const char *str, int slen = -1);
    void prepend(const char *str);
    void append(const char *str);
    bool read(std::fstream& f, int chop = -1);
    void del(int start, int count);
    void chop(int newlen);
    void insert(char *str, int start, int count = 0);
    void combinelines(std::vector<EditLine> &src);
};

enum
{
    Editor_Focused = 1,
    Editor_Used,
    Editor_Forever
};

class Editor
{
    public:
        int mode; //editor mode - 1= keep while focused, 2= keep while used in gui, 3= keep forever (i.e. until mode changes)
        bool active, rendered;
        std::string name;
        const char *filename;

        int maxx, maxy; // maxy=-1 if unlimited lines, 1 if single line editor

        bool linewrap;
        int pixelwidth; // required for up/down/hit/draw/bounds
        int pixelheight; // -1 for variable sized, i.e. from bounds()

        std::vector<EditLine> lines; // MUST always contain at least one line!

        Editor(std::string name, int mode, const char *initval) :
            mode(mode), active(true), rendered(false), name(name), filename(nullptr),
            maxx(-1), maxy(-1), linewrap(false), pixelwidth(-1), pixelheight(-1),
            cx(0), cy(0), mx(-1), my(-1), scrolly(0)
        {
            //printf("editor %08x '%s'\n", this, name);
            lines.emplace_back();
            lines.back().set(initval ? initval : "");
        }

        ~Editor()
        {
            //printf("~editor %08x '%s'\n", this, name);
            delete[] filename;

            filename = nullptr;
            clear(nullptr);
        }

        bool empty();
        void clear(const char *init = "");
        void init(const char *inittext);
        void updateheight();
        void setfile(const char *fname);
        void load();
        void save();
        void mark(bool enable);
        void selectall();
        bool region();
        EditLine &currentline();
        void copyselectionto(Editor *b);
        char *tostring();
        char *selectiontostring();
        void insert(const char *s);
        void insertallfrom(const Editor * const b);
        void scrollup();
        void scrolldown();
        void key(int code);
        void input(const char *str, int len);
        void hit(int hitx, int hity, bool dragged);
        void draw(int x, int y, int color, bool hit);
    private:
        int cx, cy; // cursor position - ensured to be valid after a region() or currentline()
        int mx, my; // selection mark, mx=-1 if following cursor - avoid direct access, instead use region()
        int scrolly; // vertical scroll offset

        void removelines(int start, int count);
        bool region(int &sx, int &sy, int &ex, int &ey);
        bool del(); // removes the current selection (if any)
        void insert(char ch);
        bool readback(std::fstream& file);
};

extern void readyeditors();
extern void flusheditors();
extern Editor *useeditor(std::string name, int mode, bool focus, const char *initval = nullptr);

#endif
