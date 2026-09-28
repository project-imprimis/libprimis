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

        /**
         * @brief Clears the text associated with this line.
         *
         * Frees the string associated with this line from the heap, and sets the
         * length and maxlength of the line to 0.
         */
        void clear();
        void set(const char *str, int slen = -1);

        /**
         * @brief Adds a string to the beginning of this line.
         *
         * Assumes the passed string is properly null terminated
         *
         * @param str the string to prepend to this line
         */
        void prepend(const char *str);

        /**
         * @brief Adds a string to the end of this line.
         *
         * Assumes the passed string is properly null terminated.
         *
         * @param str the string to append to this line
         */
        void append(const char *str);
        bool read(std::fstream& f, int chop = -1);

        /**
         * @brief Removes the specified characters from this line.
         *
         * No effect if the count is zero or less. If start is negative, sets
         * start to zero; if above the size of the string, no effect.
         *
         * @param start starting index to delete
         * @param count number of indices to delete sequentially after start
         */
        void del(int start, int count);

        /**
         * @brief Truncates this line's text to the specified length.
         *
         * Inserts a null terminator at the specified index in this line. If the
         * specified length is longer than the current string or less than zero,
         * the modified element is clamped to within the bounds of the existing
         * string.
         *
         * @param newlen index to truncate the string at
         */
        void chop(int newlen);

        /**
         * @brief Inserts a string into this line's text.
         *
         * @param str the string to insert
         * @param start the index in the line to insert at
         * @param count length of the string to insert
         */
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

        Editor(std::string initname, int initmode, const char *initval) :
            mode(initmode), active(true), rendered(false), name(initname), filename(nullptr),
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

        /**
         * @brief Clears and sets the editor's contents to the provided string.
         *
         * If the first line of the text is identical to that of the inittext message,
         * no changes will be performed.
         */
        void init(const char *inittext);

        /**
         * @brief Recalculates width and pixelheight fields.
         *
         * Uses text_boundsf to recalculate this object's width and height fields.
         */
        void updateheight();

        /**
         * @brief Sets the file that this editor is reading.
         *
         * Frees the existing file handle if it exists, and creates a new heap string to
         * contain the new file's name. If the file name is null, no allocation is performed
         * and the filename field is set to nullptr.
         *
         * @param fname the filename to set
         */
        void setfile(const char *fname);

        /**
         * @brief Loads the file specified in the filename field.
         *
         * This name is set by the `setfile()` method. If this name is set to nullptr,
         * no effect is performed. Otherwise, clears the existing editor contents
         * and loads the lines in the file pointed to by `filename`.
         */
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

        /**
         * @brief Returns heap-allocated char array containing selected contents of this editor.
         *
         * Creates a char array with size equal to the sum of the selected content's
         * values, then copies the contents of those lines to the return array.
         * Separates each line with a `\n` newline character.
         *
         * @return char array of this editor's contents
         */
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

        /**
         * @brief Constrain results to within buffer area
         *
         * constrain results (cx, cy) to within buffer - s=start, e=end, return true if a selection range
         * also ensures that cy is always within lines[] and cx is valid
         *
         * @param sx start x
         * @param sy start y
         * @param ex end x
         * @param ey end y
         *
         * @return true if start and end do not match, false if both starts and ends match
         */
        bool region(int &sx, int &sy, int &ex, int &ey);

        /**
         * @brief Removes the current selection (if any)
         *
         * @return true if region was deleted, false if no selection to delete
         */
        bool del();
        void insert(char ch);
        bool readback(std::fstream& file);
};

extern void readyeditors();
extern void flusheditors();
extern Editor *useeditor(std::string name, int mode, bool focus, const char *initval = nullptr);

#endif
