package com.kaiwalya.kuant

class Main extends akka.actor.Actor{
	val as = context.system
	val httpManager = akka.io.IO(spray.can.Http)(as)
	
	override def preStart():Unit = {
		val processor = as.actorOf(CommandProcessor.props(), "cmdprocessor")
		val console = as.actorOf(ConsoleIO.props(processor), "cmdconsole")
		context.watch(console)
		
		//val req = spray.http.HttpRequest(spray.http.HttpMethods.GET, spray.http.Uri("http://notexitstsquandl.com/api/v1/multisets.json?columns=WIKI.AAPL.4,DOE.RWTC.1&rows=1"))
		//httpManager ! req
	}

	def receive = {
		
		case akka.actor.Terminated(actor: akka.actor.ActorRef) => {
			as.shutdown()
		}
	
		case response: spray.http.HttpResponse => {
			import spray.json._
			val json = response.entity.asString.asJson
			println(json.prettyPrint)
		}
		case akka.actor.Status.Failure(x) => {
			println(x)
		}
	}
}

object App{	
	def main(args: Array[String]): Unit = {
		val as = akka.actor.ActorSystem("kuant")
		as.actorOf(akka.actor.Props[Main], "main")
	}
}