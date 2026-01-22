#ifndef TEXTEDIT_H_
#define TEXTEDIT_H_

class EditLine final
{
    public:
        enum { Chunk_Size = 256 };

        char *text;
        int len;

        EditLine() : text(nullptr), len(0), maxlen(0) {}
        EditLine(const char *init) : text(nullptr), len(0), maxlen(0)
        {
            set(init);
        }

        /**
         * @brief Returns whether the line's text has any content.
         *
         * @return true if the line has at least one character
         */
        bool empty() const;
        void clear();
        void set(const char *str, int slen = -1);
        void prepend(const char *str);
        void append(const char *str);
        bool read(std::fstream& f, int chop = -1);
        void del(int start, int count);
        void chop(int newlen);
        void insert(char *str, int start, int count = 0);
        void combinelines(std::vector<EditLine> &src);
    private:
        bool grow(int total, const char *fmt = "", ...);

        int maxlen;

};

enum
{
    Editor_Focused = 1,
    Editor_Used,
    Editor_Forever
};

class Editor final
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

        /**
         * @brief Retuns if there are no lines added to this editor.
         *
         * There will be a single blank line with contents "" created by the ctor,
         * so the presence of this line will not cause this to return "true"
         *
         * @return true if any line other than the ctor's "" has been added
         */
        bool empty() const;
        void clear(const char *init = "");
        void init(const char *inittext);
        void updateheight();
        void setfile(const char *fname);
        void load();

        /**
         * @brief Writes contents of this editor to a file.
         *
         * The file path/name to write to is located in the `filename` field. If
         * this string is null, silently fails and does nothing; otherwise, attempts
         * to write to the file at `filename`.
         *
         * Silently fails if std::fstream cannot write to the file indicated.
         */
        void save();

        /**
         * @brief Sets the selection marker.
         *
         * If enable is false, sets mx to -1; otherwise sets it to the current cursor
         * x position. mx is the value checked to determine whether a marker exists.
         *
         * Sets my, marker y position, to the current cursor position.
         *
         * @param enable whether to set disabled (-1) flag, or position of cursor
         */
        void mark(bool enable);

        /**
         * @brief Selects all text in the editor.
         *
         * Sets the marker position to the beginning of the text, and the cursor
         * to the end. This selects all of the text.
         */
        void selectall();
        bool region();
        EditLine &currentline();
        void copyselectionto(Editor *b);

        /**
         * @brief Returns heap-allocated char array containing whole contents of this editor.
         *
         * Creates a char array with size equal to the sum of all lines' values,
         * then copies the contents of those lines to the return array. Separates
         * each line with a `\n` newline character.
         *
         * @return char array of this editor's contents
         */
        char *tostring() const;
        char *selectiontostring();
        void insert(const char *s);
        void insertallfrom(const Editor *b);

        /**
         * @brief Moves the editor cursor position up one line.
         *
         * Sets the cursor y position (cy) to one line higher (reduces value of
         * cy). This can cause an underflow to negative values if cy is already
         * less than or equal to zero.
         */
        void scrollup();

        /**
         * @brief Moves the editor cursor position down one line.
         *
         * Sets the cursor y position (cy) to one line lower (increases value of
         * cy). This can cause an overflow to values above maxy if cy is already
         * greater than or equal to maxy.
         */
        void scrolldown();
        void key(int code);
        void input(const char *str, int len);
        void hit(int hitx, int hity, bool dragged);
        void draw(int x, int y, int color);
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
