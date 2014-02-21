package com.kaiwalya.kuant

object Reader {
	def props(processor: akka.actor.ActorRef): akka.actor.Props = {
		val props = akka.actor.Props(classOf[Reader], processor)
		props.withDispatcher("stdio-pinned-dispatcher")
	}
}

class Reader(processor: akka.actor.ActorRef) extends akka.actor.Actor {
	
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

object Processor {
	def props(): akka.actor.Props = {
		val props = akka.actor.Props[Processor]
		props
	}
}


class Processor extends akka.actor.Actor{
	val sPrompt = "\r\nkuant: "
	print(sPrompt)
	
	def receive = {
		case (sCmd:String) => {
			if (sCmd == "exit") {
				self ! akka.actor.PoisonPill
			}
			else {
				print("Unknown Command: " + sCmd + sPrompt)
			}
		}
	}
}

class Main extends akka.actor.Actor{
	var as: akka.actor.ActorSystem = null
	
	override def preStart():Unit = {
		as = akka.actor.ActorSystem("kuant")
		val processor = as.actorOf(Processor.props(), "cmdprocessor")
		val reader = as.actorOf(Reader.props(processor), "cmdreader")
		context.watch(reader)
	}

	def receive = {
		case akka.actor.Terminated(x) => {
			as.shutdown()
			self ! akka.actor.PoisonPill
		}
	}
}

object App{	
	def main(args: Array[String]): Unit = {
		akka.Main.main(Array("com.kaiwalya.kuant.Main"))
	}
}