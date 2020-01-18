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

static const char*  s_calendar_path = NULL;
static time_t s_start_date = 0;
static time_t s_end_date = 0;

int process_calendar( const char* calendar_path );



int main(int argc, const char * argv[])
{
    printf("calendar hour parser running...\n");
 
    const char* start_date = NULL;
    const char* end_date   = NULL;

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
                    break;
                    
                case 's':
                case 'S':
                    start_date = argv[i + 1];
                    break;

                case 'e':
                case 'E':
                    end_date = argv[i + 1];
                    break;
            }
        }
    }
    
    if( !s_calendar_path )
    {
        printf( "%s: no input calendar .ics file\n", argv[0] );
        return -1;
    }
    
    if( start_date )
    {
        struct tm time = {0};
        strptime( start_date, "%m/%d/%Y", &time );
        s_start_date = mktime( &time );
    }

    if( end_date )
    {
        struct tm time = {0};
        strptime( end_date, "%m/%d/%Y", &time );
        s_end_date = mktime( &time );
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
        {
            printf( "%s: failed to get line\n", __FUNCTION__ );
            return 0;
        }
        
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
        {
            printf( "%s: failed to get line\n", __FUNCTION__ );
            return;
        }

    } while( !is_end_of_record( *lineBuffer ) );
}


void parse_date_time_string( const char* date, struct tm* time )
{
    // for now just jump forward to the : -- which skips over the ; and then skips time zone specification (i.e. TZID=America/Los_Angeles)
    char* colon = strchr( date, ':' );
    
    if( colon && time )
        strptime( &colon[1], "%Y%m%dT%H%M%S", time );
}


int is_in_date_window( time_t start, time_t end )
{
    // no inputs means we do all dates
    if( !s_start_date && !s_end_date )
        return 1;
    
    // compare start to start, if the input date is before the start date, the diff will be negative
    int startDelta = 1;
    int endDelta = 1;

    if( s_start_date )
        startDelta = difftime( start, s_start_date );
    
    if( s_end_date )
        endDelta = difftime( s_end_date, end );
    
    return startDelta >= 0 && endDelta >= 0;
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

            time_t startTime = mktime( &start );
            time_t endTime   = mktime( &end );

            double duration = difftime( endTime, startTime ) / (60 * 60);
            
            // see if we should output this record
            if( is_in_date_window( startTime, endTime ) )
            {
                total_hours += duration;

                char dateString[1024] = {};
                strftime( dateString, sizeof(dateString), "%m/%d/%Y", &start );
                printf( "%s: %g hrs - ", dateString, duration );

                if( find_record( "SUMMARY", &lineBuffer, &bufSize, calFile ) )
                    printf( "%s", lineBuffer );

                printf( "\n" );
            }

            skip_record( &lineBuffer, &bufSize, calFile );
        }
    }
    while( lineSize >= 0 );
    
    printf( "total hours: %g\n", total_hours );

    fclose( calFile );
    return 0;
}

