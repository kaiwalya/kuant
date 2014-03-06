package com.kaiwalya.kuant;

import java.io.Serializable;

import akka.actor.ActorSystem;
import akka.actor.Props;
import akka.actor.UntypedActor;

/**
 * Hello world!
 *
 */
public class App extends UntypedActor
{
	public static class PrintCmd implements Serializable{}
	public static class ExitCmd implements Serializable{}
	
    public static void main( String[] args )
    {
    	create().awaitTermination();
    }
    
    public static ActorSystem create() {
    	ActorSystem sys = ActorSystem.create("kuant");
    	sys.actorOf(Props.create(App.class));
    	return sys;
    }

    @Override
    public void preStart() throws Exception {
    	super.preStart();
    	self().tell(new PrintCmd(), self());
    }
    
    @Override
    public void onReceive(Object arg0) throws Exception {
    	if (arg0 instanceof PrintCmd) {
    		System.out.println("Hello World!");
    		self().tell(new ExitCmd(), self());
    	}
    	else if (arg0 instanceof ExitCmd) {
    		context().system().shutdown();
    	}
    }
}
