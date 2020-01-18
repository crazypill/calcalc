//
//  main.c
//  calcalc
//
//  Created by Alex Lelievre on 1/17/20.
//  Copyright © 2020 Alex Lelievre. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char* s_calendar_path = NULL;

int process_calendar( const char* calendar_path );



int main(int argc, const char * argv[])
{
    // insert code here...
    printf("Calendar hour parser running...\n");
    
    // look at args
    for( int i = 1; i < argc; i++ )
    {
        // look for command, single letter
        if( argv[i][0] == '-' )
        {
            switch( argv[i][1] )
            {
                case 'f':
                case 'F':
                    // capture filename of exported calendar
                    s_calendar_path = argv[i + 1];
                    printf( "got path: %s\n", s_calendar_path );
                    break;
            }
        }
    }
    
    if( !s_calendar_path )
    {
        printf( "%s: no input calendar .ics file\n", argv[0] );
        return -1;
    }
    
    // process file looking for calendar events
    return process_calendar( s_calendar_path );
}


int find_key( const char* key, const char* line )
{
    // this just looks to see if the line starts with the input key, if so- it returns the rest of the string in result...
    return strncmp( key, line, strlen( key ) ) == 0;
}


int is_end_of_record( const char* line )
{
    return strncmp( "END:VEVENT", line, strlen( "END:VEVENT" ) ) == 0;
}


int find_record( const char* key, char** lineBuffer, size_t* buf_size, FILE* cal_file )
{
    fpos_t filePos;
    fgetpos( cal_file, &filePos );
    int found = 0;
    
    do
    {
        ssize_t lineSize = getline( lineBuffer, buf_size, cal_file );
        if( lineSize < 0 )
            return 0;
        
        // okay we found the start of a calendar record, let's find the start and end times
        if( find_key( key, *lineBuffer ) )
        {
            found = 1;
            break;
        }
    } while( !is_end_of_record( *lineBuffer ) );
    
    // rewind to where we started for subsequent searches
    fsetpos( cal_file, &filePos );
    return found;
}

void skip_record( char** lineBuffer, size_t* buf_size, FILE* cal_file )
{
    do
    {
        ssize_t lineSize = getline( lineBuffer, buf_size, cal_file );
        if( lineSize < 0 )
            return;

    } while( !is_end_of_record( *lineBuffer ) );
}


void parse_date_time_string( const char* date, struct tm* time )
{
    // for now just jump forward to the : -- which skips over the ; and then skips time zone specification (i.e. TZID=America/Los_Angeles)
    char* colon = strchr( date, ':' );
    
    if( colon && time )
        strptime( &colon[1], "%Y%m%dT%H%M%S", time );
}


int process_calendar( const char* calendar_path )
{
    // open the file and process it line by line with a mild state machine
    FILE* calFile = fopen( calendar_path, "r" );
    if( !calFile )
        return -1;
    
    char* lineBuffer = NULL;
    size_t bufSize = sizeof( lineBuffer );
    ssize_t lineSize = 0;
    double total_hours = 0;
    
    do
    {
        lineSize = getline( &lineBuffer, &bufSize, calFile );
        
        // find the beginning of the record, and mark the file location, we will rewind here for each record look up
        if( find_key( "BEGIN:VEVENT", lineBuffer ) )
        {
            struct tm start = {};
            struct tm end   = {};
            
            if( find_record( "DTSTART", &lineBuffer, &bufSize, calFile ) )
                parse_date_time_string( lineBuffer, &start );

            if( find_record( "DTEND", &lineBuffer, &bufSize, calFile ) )
                parse_date_time_string( lineBuffer, &end );

            double delta = difftime( mktime(&end), mktime(&start) ) / (60 * 60);
            total_hours += delta;
            
            char dateString[1024] = {};
            strftime(dateString, sizeof(dateString), "%m/%d/%Y", &start);
            printf( "%s: %g - ", dateString, delta );

            if( find_record( "SUMMARY", &lineBuffer, &bufSize, calFile ) )
                printf( "%s", lineBuffer );

            skip_record( &lineBuffer, &bufSize, calFile );
            printf( "\n" );
        }
    }
    while( lineSize >= 0 );
    
    printf( "total hours: %g\n", total_hours );

    fclose( calFile );
    return 0;
}

