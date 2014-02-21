package com.kaiwalya.kuant

class Main extends akka.actor.Actor{
	var as: akka.actor.ActorSystem = null
	
	override def preStart():Unit = {
		as = akka.actor.ActorSystem("kuant")
		val processor = as.actorOf(CommandProcessor.props(), "cmdprocessor")
		val console = as.actorOf(ConsoleIO.props(processor), "cmdconsole")
		context.watch(console)
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