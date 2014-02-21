package com.kaiwalya.kuant

import akka.actor.Actor

object CommandProcessor {
	def props(): akka.actor.Props = {
		val props = akka.actor.Props[CommandProcessor]
		props
	}
}
class CommandProcessor extends Actor {
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