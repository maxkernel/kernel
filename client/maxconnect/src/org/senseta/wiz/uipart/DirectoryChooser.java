package org.senseta.wiz.uipart;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Frame;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.Dialog.ModalityType;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.File;
import java.util.Arrays;
import java.util.Enumeration;

import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JRootPane;
import javax.swing.JScrollPane;
import javax.swing.JTree;
import javax.swing.KeyStroke;
import javax.swing.filechooser.FileSystemView;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.TreeCellRenderer;
import javax.swing.tree.TreePath;
import javax.swing.tree.TreeSelectionModel;

import org.senseta.wiz.ImageCache;

public class DirectoryChooser extends JTree
{
	private static final long serialVersionUID = 1107310442551777971L;
	private static final FileSystemView view = FileSystemView.getFileSystemView();
	
	private DirectoryChooser(File show)
	{
		super(new RootListNode(view.getRoots()));
		setRootVisible(false);
		getSelectionModel().setSelectionMode(TreeSelectionModel.SINGLE_TREE_SELECTION);
		setCellRenderer(new CellDisplay());
		setEditable(false);
		setSelected(show);
	}
	
	public File getSelected()
	{
		return ((FileNode)getLastSelectedPathComponent()).getFile();
	}
	
	public void setSelected(File file)
	{
		if (file == null)
		{
			file = view.getDefaultDirectory();
		}
		setSelectionPath(mkPath(file));
	}

	
	private TreePath mkPath(File file)
	{
		TreePath parentPath = null;
		Object root = getModel().getRoot();
		if (root instanceof FileNode && (((FileNode)root).getFile().equals(file)))
		{
			return new TreePath(root);
		}
		else if (root instanceof RootListNode && (((RootListNode)root).hasFile(file)))
		{
			parentPath = new TreePath(root);
		}
		
		
		if (parentPath == null)
		{
			parentPath = mkPath(view.getParentDirectory(file));
		}
		
		DefaultMutableTreeNode parentNode = (DefaultMutableTreeNode)parentPath.getLastPathComponent();
		Enumeration<?> enumeration = parentNode.children();
		while (enumeration.hasMoreElements()) {
			FileNode child = (FileNode)enumeration.nextElement();
			if (child.getFile().equals(file)) {
				return parentPath.pathByAddingChild(child);		
			}
		}
		return null;
	}
	
	private class CellDisplay implements TreeCellRenderer
	{
		private JLabel label = new JLabel();
		
		@Override
		public Component getTreeCellRendererComponent(JTree tree, Object value, boolean selected, boolean expanded, boolean leaf, int row, boolean hasFocus) {
			FileNode n = (FileNode)value;
			label.setText(view.getSystemDisplayName(n.getFile()));
			label.setIcon(view.getSystemIcon(n.getFile()));
			
			return label;
		}
	}
	
	private static class FileNode extends DefaultMutableTreeNode
	{
		private static final long serialVersionUID = 0L;

		private FileNode(File fp)
		{
			super(fp);
		}

		private File getFile() { return (File)userObject; }
		public int getChildCount() { populateChildren(); return super.getChildCount(); }
		public Enumeration<?> children() { populateChildren(); return super.children(); }
		
		public boolean isLeaf()
		{
			if (!view.isTraversable(getFile()) || getFile().listFiles() == null)
				return true;
			
			
			for (File f : getFile().listFiles())
			{
				if (view.isTraversable(f))
					return false;
			}
			
			return true;
		}
		
		private void populateChildren() {
			if (children == null) {
				File[] files = view.getFiles(getFile(), true);
				Arrays.sort(files);
				for (File f : files) {
					if (view.isTraversable(f))
					{
						insert(new FileNode(f), (children == null) ? 0 : children.size());
					}
				}
			}
		}
	}
	
	private static class RootListNode extends DefaultMutableTreeNode
	{
		private static final long serialVersionUID = 0L;

		private RootListNode(File[] fps)
		{
			super(fps);
		}

		private File[] getFiles() { return (File[])userObject; }
		public int getChildCount() { populateChildren(); return super.getChildCount(); }
		public Enumeration<?> children() { populateChildren(); return super.children(); }
		public boolean isLeaf() { return false; }
		
		public boolean hasFile(File fp)
		{
			for (File f : getFiles())
			{
				if (f.equals(fp))
					return true;
			}
			return false;
		}
		
		private void populateChildren() {
			if (children == null) {
				Arrays.sort(getFiles());
				for (File f : getFiles()) {
					insert(new FileNode(f), (children == null) ? 0 : children.size());
				}
			}
		}
	}
	
	
	public static DirectoryChooser showDirectories(File show, String title, String desc, Frame owner, final ActionListener listener)
	{
		final JLabel titlelabel = new JLabel(title);
		final DirectoryChooser tree = new DirectoryChooser(show);
		final JButton okay = new JButton("Okay", new ImageIcon(ImageCache.fromCache("sign_tick")));
		final JScrollPane scroll = new JScrollPane(tree);
		
		final JDialog d = new JDialog(owner, title) {
			private static final long serialVersionUID = 0L;
			protected JRootPane createRootPane() {
				ActionListener actionListener = new ActionListener() {
					public void actionPerformed(ActionEvent actionEvent) {
						dispose();
					}
				};
				JRootPane rootPane = new JRootPane();
				KeyStroke stroke = KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0);
				rootPane.registerKeyboardAction(actionListener, stroke, JComponent.WHEN_IN_FOCUSED_WINDOW);
				return rootPane;
			}
		};
		d.addWindowListener(new WindowAdapter() {
			@Override
			public void windowClosed(WindowEvent e) {
				listener.actionPerformed(new ActionEvent(tree, e.getID(), ""));
			}
			
			@Override public void windowClosing(WindowEvent e) { d.dispose(); }
		});
		d.setModalityType(ModalityType.APPLICATION_MODAL);
		d.setLayout(new LayoutManager() {
			@Override
			public void layoutContainer(Container parent) {
				Insets in = parent.getInsets();
				int x = in.left, y = in.top;
				int w = parent.getWidth() - in.left - in.right;
				int h = parent.getHeight() - in.top - in.bottom;
				
				titlelabel.setBounds(x, y, w, 25);
				scroll.setBounds(x, y+25, w, h-55);
				okay.setBounds(w-105, h-30, 100, 25);
			}

			@Override public void addLayoutComponent(String name, Component comp) {}
			@Override public void removeLayoutComponent(Component comp) {}
			@Override public Dimension minimumLayoutSize(Container parent) { return new Dimension(0,0); }
			@Override public Dimension preferredLayoutSize(Container parent) { return minimumLayoutSize(parent); }
		});
		d.add(titlelabel);
		d.add(scroll);
		d.add(okay);
		
		okay.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				d.dispose();
			}
		});
		
		d.setSize(300, 400);
		d.setLocationRelativeTo(owner);
		d.setVisible(true);
		
		return tree;
	}
}
