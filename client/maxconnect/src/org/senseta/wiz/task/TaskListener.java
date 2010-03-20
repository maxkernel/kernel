package org.senseta.wiz.task;

import java.util.EventListener;

import org.senseta.wiz.Prompt;

public interface TaskListener<T> extends EventListener
{
	public void taskAppendOutput(String text);
	public void taskShowPrompt(Prompt prompt);
	public void taskComplete(T value, boolean iserror);
}
