/*************************************************************************
 *
 * Copyright (c) 2010 Kohei Yoshida
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 ************************************************************************/

#include "inputparser.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <string>
#include <iostream>

using namespace std;
using namespace ixion;

bool parse_model_input(const string& fpath)
{
    model_parser parser(fpath);
    try
    {
        parser.parse();
        const vector<cell>& cells = parser.get_cells();
        for (size_t i = 0; i < cells.size(); ++i)
            cout << cells[i].print() << endl;
    }
    catch (const exception& e)
    {
        cout << e.what() << endl;
        return false;
    }
    return true;
}

int main (int argc, char** argv)
{
    /* Flag set by '--verbose'. */
    static int verbose_flag;
    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"verbose", no_argument,       &verbose_flag, 1},
        {"brief",   no_argument,       &verbose_flag, 0},
        /* These options don't set a flag.
           We distinguish them by their indices. */
        {"add",     no_argument,       0, 'a'},
        {"append",  no_argument,       0, 'b'},
        {"delete",  required_argument, 0, 'd'},
        {"create",  required_argument, 0, 'c'},
        {"file",    required_argument, 0, 'f'},
        {0, 0, 0, 0}
    };

    while (true)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;
        int c = getopt_long (argc, argv, "abc:d:f:", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                printf ("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
                break;

            case 'a':
                puts ("option -a\n");
                break;

            case 'b':
                puts ("option -b\n");
                break;

            case 'c':
                printf ("option -c with value `%s'\n", optarg);
                break;

            case 'd':
                printf ("option -d with value `%s'\n", optarg);
                break;

            case 'f':
                printf ("option -f with value `%s'\n", optarg);
                break;

            case '?':
                /* getopt_long already printed an error message. */
                break;

            default:
                abort ();
        }
    }

    if (verbose_flag)
        puts ("verbose flag is set");

    if (argc - optind > 1)
    {
        fprintf(stderr, "takes exactly one argument\n");
        exit (EXIT_FAILURE);
    }

    string fpath = argv[optind];
    cout << fpath << endl;

    if (!parse_model_input(fpath))
        exit (EXIT_FAILURE);
}

