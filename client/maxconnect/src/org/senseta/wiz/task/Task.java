package org.senseta.wiz.task;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.locks.LockSupport;

import org.senseta.wiz.Prompt;
import org.senseta.wiz.Prompt.Button;

public abstract class Task<T> implements Callable<T>
{
	private TaskListener<T> listener;
	private Throwable error;
	
	public Task(TaskListener<T> listener)
	{
		this.listener = listener;
		error = null;
	}
	
	public final void start()
	{
		new Thread(new Runnable() {
			@Override
			public void run()
			{
				FutureTask<T> future = new FutureTask<T>(Task.this);
				future.run();
				try {
					T value = future.get();
					listener.taskComplete(value, false);
					return;
				}
				catch (ExecutionException e)
				{
					error = e.getCause();
				}
				catch (InterruptedException e)
				{
					error = e;
				}
				
				listener.taskComplete(null, true);
			}
		}).start();
	}
	
	public Throwable getError()
	{
		return error;
	}
	
	protected void appendln(String text)
	{
		append(text+"\n");
	}
	
	protected void append(String text)
	{
		listener.taskAppendOutput(text);
	}
	
	protected Button show(Prompt prompt)
	{
		final Thread blocked = Thread.currentThread();
		prompt.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e)
			{
				LockSupport.unpark(blocked);
			}
		});
		
		listener.taskShowPrompt(prompt);
		LockSupport.park();
		
		return prompt.getSelected();
	}
}
