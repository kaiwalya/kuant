package com.kaiwalya.kuant

import akka.actor.Actor
import akka.actor.ActorRef


object ConsoleIO {
	def props(processor: akka.actor.ActorRef): akka.actor.Props = {
		val props = akka.actor.Props(classOf[ConsoleIO], processor)
		props.withDispatcher("stdio-pinned-dispatcher")
	}
}

class ConsoleIO(processor: ActorRef) extends Actor {
	
	var str = ""
	var exit = false
		
	override def preStart():Unit = {
		self ! 0
		context.watch(processor)
	}
	
	private def shutdown():Unit = {
		context.become({
			case _ => {}
		}, true)
		self ! akka.actor.PoisonPill
	}
	
	private def proceed(): Unit = {
		var c: Char = 0
		while (System.in.available() > 0) {
			c = (System.in.read()).asInstanceOf[Char]
			
			//EOF
			if (c == 0xFFFF) {
				return shutdown()
			}
			
			if (c == '\n') {
				val s = str.asInstanceOf[String]
				processor ! s
				str = ""
			}
			else str = str + c
		}
		java.lang.Thread.sleep(100)
		self ! 0
	}
	
	def receive = {
		case 0 => this.proceed
		case akka.actor.Terminated(_) => this.shutdown
	}
}