/*
 *    ALMA - Atacama Large Millimiter Array
 *    (c) European Southern Observatory, 2002
 *    Copyright by ESO (in the framework of the ALMA collaboration)
 *    and Cosylab 2002, All rights reserved
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 2.1 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *    MA 02111-1307  USA
 */
package alma.acs.logging.tools;

import java.io.File;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.regex.Pattern;

import alma.acs.util.CmdLineArgs;
import alma.acs.util.CmdLineOption;
import alma.acs.util.CmdLineRegisteredOption;

/**
 * A coommand line tool to perform some helpful operation
 * with the files of logs
 * 
 * @author acaproni
 *
 */
public class LogAssistant {
	
	// The format of the date is the same as the ILogEntry without msecs
	public static final String TIME_FORMAT = "yyyy'-'MM'-'dd'T'HH':'mm':'ss";
	
	// The command to execute
	private char command; // -x, -p, -h
	
	// The start and end date for extraction
	private Date startDate=null, endDate=null;
	
	// The name of the filter file for extraction
	private String filterFileName=null;
	
	// The number of logs per file for splitting
	private Integer num=null;
	
	// The number of minutes per file while splitting
	private Integer minutes=null;
	
	// The name of the source file
	private String sourceFileName=null;
	
	// The name of the file generated by the process
	//
	// While splitting the process generates more file appending a sequenzial 
	// number to this string 
	private String destFileName=null;
	
	// True if the user needs the output in CSV format
	private boolean outputAsCSV=false;
	
	// The index and order of the cols to write if the output is CSV
	// see CSVConverter
	private String cols=null;
	
	/**
	 * Constructor
	 * 
	 * @param args The command line params
	 */
	public LogAssistant(String[] args) {
		// Parse the command line
		try {
			parseCommandLine(args);
		} catch (IllegalStateException e) {
			System.err.println("Error in the parameters: "+e.getMessage());
			usage("acsLogAssistant");
		}
		if (command=='h') {
			usage("acsLogAssistant");
			return;
		}
		if (checkState()) {
			if (command=='x') {
				extractLogs();
			} else if (command=='p') {
				splitFile();
			}
		}
	}
	
	/**
	 * Parse the command line and fill the internal variables
	 * Throws an IllegalStateException if an error arises while parsing
	 * like for example invalid parameters.
	 * 
	 * @param params The parameters in the command line
	 * @throws IllegalStateException If the parameters in the command line are invalid
	 */
	private void parseCommandLine(String[] params) throws IllegalStateException {
		if (params.length<5) {
			// The param must be at least 5:
			// -command (-split)
			// at least one parameter for the command and its value (-time val)
			// source
			// destination
			throw new IllegalStateException("Wrong number of params");
		}
		CmdLineArgs cmdLineArgs = new CmdLineArgs();
		CmdLineRegisteredOption extractCmd = new CmdLineRegisteredOption("-x","-extract",0);
		cmdLineArgs.registerOption(extractCmd);
		CmdLineRegisteredOption splitCmd = new CmdLineRegisteredOption("-p","-split",0);
		cmdLineArgs.registerOption(splitCmd);
		CmdLineRegisteredOption helpCmd = new CmdLineRegisteredOption("-h","-help",0);
		cmdLineArgs.registerOption(helpCmd);
		CmdLineRegisteredOption csvOtuputFormat = new CmdLineRegisteredOption("-c","-csv",0);
		cmdLineArgs.registerOption(csvOtuputFormat);
		CmdLineRegisteredOption startTime = new CmdLineRegisteredOption("-s","-start",1);
		cmdLineArgs.registerOption(startTime);
		CmdLineRegisteredOption endTime = new CmdLineRegisteredOption("-e","-end",1);
		cmdLineArgs.registerOption(endTime);
		CmdLineRegisteredOption filterName = new CmdLineRegisteredOption("-f","-filter",1);
		cmdLineArgs.registerOption(filterName);
		CmdLineRegisteredOption time = new CmdLineRegisteredOption("-t","-time",1);
		cmdLineArgs.registerOption(time);
		CmdLineRegisteredOption number = new CmdLineRegisteredOption("-n","-num",1);
		cmdLineArgs.registerOption(number);
		CmdLineRegisteredOption columns = new CmdLineRegisteredOption("-l","-col",1);
		cmdLineArgs.registerOption(columns);
		cmdLineArgs.parseArgs(params);
		
		// Command==Extract
		if (cmdLineArgs.isSpecified(extractCmd)) {
			command='x';
		}
		// Command==split
		if (cmdLineArgs.isSpecified(splitCmd)) {
			command='p';
		}
		// Command==help
		if (cmdLineArgs.isSpecified(helpCmd)) {
			command='h';
		}
		if (command=='h') {
			return;
		}
		// Start date
		if (cmdLineArgs.isSpecified(startTime)) {
			String[] val = cmdLineArgs.getValues(startTime);
			if (val==null || val.length<1) {
				throw new IllegalStateException("Start date missing/wrong "+TIME_FORMAT);
			}
			try {
				startDate=getDate(val[0]);
			} catch (ParseException e) {
				throw new IllegalStateException("Wrong date format "+TIME_FORMAT);
			}
		}
		// End date
		if (cmdLineArgs.isSpecified(endTime)) {
			String[] val = cmdLineArgs.getValues(endTime);
			if (val==null || val.length<1) {
				throw new IllegalStateException("End date missing/wrong "+TIME_FORMAT);
			}
			try {
				endDate=getDate(val[0]);
			} catch (ParseException e) {
				throw new IllegalStateException("Wrong date format "+TIME_FORMAT);
			}
		}
		// Filter name
		if (cmdLineArgs.isSpecified(filterName)) {
			String[] val = cmdLineArgs.getValues(filterName);
			if (val==null || val.length<1) {
				throw new IllegalStateException("Wrong or missing filter name");
			} 
			filterFileName=val[0];
		}
		// Time
		if (cmdLineArgs.isSpecified(time)) {
			String[] val = cmdLineArgs.getValues(time);
			if (val==null || val.length<1) {
				throw new IllegalStateException("Wrong or missing time (minutes)");
			} 
			try {
				minutes=Integer.parseInt(val[0]);
			} catch (NumberFormatException e) {
				throw new IllegalStateException("Wrong format for the time (minutes)");
			}
		}
		// Number
		if (cmdLineArgs.isSpecified(number)) {
			String[] val = cmdLineArgs.getValues(number);
			if (val==null || val.length<1) {
				throw new IllegalStateException("Wrong or missing time (minutes)");
			} 
			try {
				num=Integer.parseInt(val[0]);
			} catch (NumberFormatException e) {
				throw new IllegalStateException("Wrong format for the number of logs");
			}
		}
		// Col
		if (cmdLineArgs.isSpecified(columns)) {
			String[] val = cmdLineArgs.getValues(columns);
			if (val==null || val.length<1) {
				throw new IllegalStateException("Wrong or missing time (minutes)");
			} 
			if (!Pattern.matches("[0-9a-g]+",val[0])) {
				throw new IllegalStateException("Wrong format for columns: [0-9a-g]+");
			}
			cols=val[0];
		}
		// CSV
		outputAsCSV=cmdLineArgs.isSpecified(csvOtuputFormat);
		// SOURCE
		sourceFileName=params[params.length-2];
		// DESTINATION
		destFileName=params[params.length-1];
	}
	
	/**
	 * Check the state of the variables.
	 * This method checks if the interal variables are set in the right way
	 * It is usually executed before running a command
	 * 
	 * @return If the state is correct
	 */
	private boolean checkState() {
		if (command!='x' && command!='p' && command!='h') {
			System.out.println("Wrong command "+command);
			return false;
		}
		if (command=='x') {
			if (endDate==null && startDate==null && filterFileName==null) {
				System.out.println("Selected extraction withouth criteria");
				System.out.println("Please, set start date and/or end date and/or a filter file name");
				return false;
			}
			if (filterFileName!=null) {
				if (filterFileName.length()==0) {
					System.out.println("Invalid empty name for the filter");
					return false;
				}
				File filterFile = new File(filterFileName);
				if (!filterFile.canRead()) {
					System.out.println(filterFileName+" is unreadable");
					return false;
				}
			}
			if (minutes!=null || num!=null) {
				System.out.println("Warning: minutes and number of logs are ignored while eXtracting");
			}
		} else if (command=='p') {
			if (num==null && minutes==null) {
				System.out.println("Splitting selected without criteria");
				System.out.println("Please, set the number of log or the time");
				return false;
			}
			if (num!=null && minutes!=null) {
				System.out.println("Set only one between number of logs and time to sPlit");
				return false;
			}
			if (startDate!=null || endDate!=null || filterFileName!=null) {
				System.out.println("Warning: start date, end date and the filter are ignored while sPlitting");
			}
		}
		// Check if the input file exists and is readable
		if (sourceFileName==null || sourceFileName.length()==0) {
			System.out.println("Invalid source file name: "+sourceFileName);
		}
		File inF = new File(sourceFileName);
		if (!inF.canRead()) {
			System.out.println(sourceFileName+" is unreadable");
			return false;
		}
		return true;
	}

	/**
	 * Parse the given string into a Date
	 * 
	 * @param date The string representing the date
	 * @return A Date obtained parsing the string
	 * @throws ParseException If an error happen getting the date from the string
	 */
	private Date getDate(String date) throws ParseException {
		SimpleDateFormat df = new SimpleDateFormat(TIME_FORMAT);
		return df.parse(date);
	}
	
	/**
	 * Extract the logs from the source to the destination file
	 *
	 */
	private void extractLogs() {
		try {
			LogFileExtractor extractor = new LogFileExtractor(sourceFileName,destFileName,startDate,endDate,filterFileName);
			extractor.extract();
		} catch (Exception e) {
			System.err.println("Error extracting: "+e.getMessage());
		}
	}
	
	/**
	 * Split the input log file in several files
	 *
	 */
	private void splitFile() {
		try {
			LogFileSplitter fileSplitter = new LogFileSplitter(sourceFileName,destFileName,num,minutes);
			fileSplitter.split();
		} catch (Exception e) {
			System.err.println("Error splitting: "+e.getMessage());
			e.printStackTrace();
		}
	}
	
	/**
	 * Print a usage message on screen
	 * 
	 * @param prgName The program name
	 */
	private static void usage(String prgName) {
		System.out.println("USAGE: "+prgName+" command command_params options src dest");
		System.out.println("command:");
		System.out.println("\t-extract|-x: extract logs depending on the command_params criteria");
		System.out.println("\tcommand_params for extraction (applied as AND):");
		System.out.println("\t\t-start|-s <start_date>: selects all the logs generated after the given date");
		System.out.println("\t\t-end|-e <end_date>: selects all the logs generated before the given date");
		System.out.println("\t\t-filter|-f <filter.xml>: selects all the logs that matches the filters specified in filter.xml");
		System.out.println("\t-split|-p: split the source file depending on the command params criteria");
		System.out.println("\tcommand_params for splitting:");
		System.out.println("\t\t-time|-t <min>: split by time in minutes");
		System.out.println("\t\t-num|-n  <num>: split by number of logs");
		System.out.println("\t-help|-h: print this help\n");
		System.out.println("options:");
		System.out.println("\t-csv|-c: write the output as CVS");
		System.out.println("\t-col|-l columns: select the columns to write in the csv (only if csv output is selected)");
		System.out.println("src: source file");
		System.out.println("dest: output file");
		System.out.println("See ACS manual for further details");
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		new LogAssistant(args);
	}

}
