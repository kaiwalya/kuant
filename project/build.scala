import sbt._
import Keys._

object KuantBuild extends Build {
	val mySettings = Seq(
		name := "kuant",
		version := "0.0.1",
		scalaVersion := "2.10.3",
		libraryDependencies += "org.apache.httpcomponents" % "httpclient" % "4.3.2",
		resolvers += "Typesafe Repository" at "http://repo.typesafe.com/typesafe/releases/",
		libraryDependencies += "com.typesafe.akka" %% "akka-actor" % "2.2.3",
		resolvers += "spray repo" at "http://repo.spray.io",
		libraryDependencies += "io.spray" % "spray-client" % "1.2.0"
	)
	lazy val root: Project = Project(id = "kuant",
		base = file("."),
		settings = Project.defaultSettings ++ mySettings)
}
		
