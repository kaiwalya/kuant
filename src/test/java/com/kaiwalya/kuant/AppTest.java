package com.kaiwalya.kuant;

import java.util.concurrent.TimeoutException;

import scala.concurrent.duration.Duration;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

/**
 * Unit test for simple App.
 */
public class AppTest 
    extends TestCase
{
    /**
     * Create the test case
     *
     * @param testName name of the test case
     */
    public AppTest( String testName )
    {
        super( testName );
    }

    /**
     * @return the suite of tests being tested
     */
    public static Test suite()
    {
        return new TestSuite( AppTest.class );
    }

    /**
     * Rigourous Test :-)
     */
    public void testAppDoesntQuicklyExit()
    {	
    	try {
	    	Duration d = Duration.create("0ms");
			App.create().awaitTermination(d);
    	}
    	catch(Exception e) {
    		assertTrue(e instanceof TimeoutException);
    		return;
    	}
    	assertTrue(false);
    }
    
    public void testAppDoesExit()
    {
    	Duration d = Duration.create("1 day");
		App.create().awaitTermination(d);
    }
}
